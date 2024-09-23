# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import http.server
import json
import logging
import mimetypes
import os
import re
from urllib import parse as urllib_parse
from typing import Callable, Dict, List, NamedTuple, Optional, Union
from xml.dom import minidom

from cca import build
from cca import cli
from cca import util


def _get_root_relative_path(request_path: str) -> str:
    request_path = os.path.dirname(request_path)
    return os.path.relpath("/", f"/{request_path}")


# Replaces all chrome:// reference to /chrome_stub/.
# Also replaces all //resources/ references to /chrome_stub/resources/, since
# some imports in cros_component and mwc use // instead of chrome://, but
# replacing all '//' is too broad.
def _stub_chrome_url(request_path: str, s: str) -> str:
    chrome_stub_path = os.path.join(_get_root_relative_path(request_path),
                                    "chrome_stub/")
    return s.replace("chrome://", chrome_stub_path).replace(
        "//resources/", os.path.join(chrome_stub_path, "resources/"))


class _Route(NamedTuple):
    # The url pattern for the route. Can be a regex.
    pattern: Union[str, re.Pattern]
    # Handler of the route, takes path as argument and returns response in
    # bytes.
    handler: Callable[[str], bytes]


class RequestHandler:
    def __init__(
        self,
        cca_root: str,
        tsc_root: str,
        gen_dir: str,
    ):
        self._cca_root = cca_root
        self._tsc_root = tsc_root
        self._gen_dir = gen_dir
        self._dev_static_dir = os.path.join(self._cca_root, "utils/cca/static")
        self._directory = self._cca_root
        self.routes = self._build_routes()

    def _load_grd_strings(self) -> Dict[str, str]:
        def get_message_text_content(message: minidom.Element) -> str:
            pieces = []
            for child in message.childNodes:
                if child.nodeType == minidom.Element.TEXT_NODE:
                    pieces.append(child.nodeValue)
                if child.nodeType == minidom.Element.ELEMENT_NODE:
                    if child.tagName == "ex":
                        continue
                    pieces.append(get_message_text_content(child))
            return "".join(pieces)

        strings = {}
        grd_path = os.path.join(self._cca_root, "strings/camera_strings.grd")
        dom = minidom.parse(grd_path)
        messages = dom.getElementsByTagName("messages")[0]
        for message in messages.getElementsByTagName("message"):
            name = message.getAttribute("name").lower()
            value = get_message_text_content(message).strip()
            assert name.startswith("ids_")
            id = name[len("ids_"):]
            strings[id] = value
        return strings

    def _handle_strings_m_js(self, request_path: str) -> bytes:
        load_time_data = {
            "board_name": "local-dev",
            "browser_version": "local-dev",
            "device_type": "local-dev",
            "is_test_image": True,
            "os_version": "local-dev",
            "textdirection": "ltr",
            "cca_disallowed": False,
            "timeLapse": True,
            "digital_zoom": True,
            "preview_ocr": True,
            "super_res": True,
        }
        load_time_data.update(self._load_grd_strings())
        relative_path = _get_root_relative_path(request_path)

        return (
            "import {loadTimeData} from "
            f"'{relative_path}/chrome_stub/resources/js/load_time_data.js';"
            f"loadTimeData.data = {json.dumps(load_time_data)}").encode()

    def _handle_preload_images_js(self, request_path: str) -> bytes:
        # TODO(pihsun): With watch, we can cache the result and only
        # re-generate when any image files are changed.
        return _stub_chrome_url(request_path,
                                build.gen_preload_images_js()).encode()

    def _transform_html(self, request_path: str, html: str) -> str:
        name = self._load_grd_strings()["name"]

        html = html.replace("$i18n{name}", name)
        html = _stub_chrome_url(request_path, html)

        relative_path = _get_root_relative_path(request_path)
        html = re.sub(r"(href|src)=\"/", f'\\1="{relative_path}/', html)

        return html

    def _transform_init_js(self, request_path: str, js: str) -> str:
        # Note that this breaks source map, but there's no frequent need to
        # debug init.ts so the impact should be minimal.
        return self._transform_js(request_path,
                                  "import './local_dev_overrides.js';\n" + js)

    def _transform_js(self, request_path: str, js: str) -> str:
        return _stub_chrome_url(request_path, js)

    def _transform_css(self, request_path: str, css: str) -> str:
        relative_path = _get_root_relative_path(request_path)
        css = re.sub(r"url\(/", f"url({relative_path}/", css)
        return css

    def _load_mojo_enums(self, mojo_file_name) -> Dict[str, Dict[str, int]]:
        with open(os.path.join(self._cca_root, "../", mojo_file_name),
                  "r") as f:
            mojom = f.read()
        enum_blocks = re.findall(r"enum (.*?) \{(.*?)\}", mojom, re.DOTALL)

        def parse_enum(enum_block: str) -> Dict[str, int]:
            count = 0
            enum = {}
            for line in enum_block.splitlines():
                line = line.strip()
                if not line or line.startswith("//"):
                    continue
                match = re.match(r"(.*?)(?: = (\d+))?,", line)
                if match is None:
                    continue
                name = match.group(1)
                value = match.group(2)
                if value is None:
                    value = count
                count += 1
                enum[name] = value
            return enum

        return {name: parse_enum(block) for name, block in enum_blocks}

    def _transform_js_mojo_js(self, request_path: str, ts_src: str) -> str:
        del request_path  # Unused.
        export_blocks = re.findall(r"export \{(.*?)\}", ts_src, re.DOTALL)
        exports = [
            export.strip() for block in export_blocks
            for export in block.split(",")
        ]
        # Ignore empty value from split.
        exports = [export for export in exports if export]

        def get_import_export_names(export):
            # Each `export` is either a single name that is used both as import
            # and export name, or are in form "A as B".
            tokens = export.split()
            return [tokens[0], tokens[-1]]

        exports = [get_import_export_names(export) for export in exports]

        # Stub the real enum values for enum in mojom files, since those enum
        # values are used by CCA.
        mojo_files = [
            "camera_app_helper.mojom",
            "events_sender.mojom",
            "ocr.mojom",
            "types.mojom",
        ]
        # TODO(pihsun): This doesn't handle possible enum name collision
        # between different mojom files.
        mojo_enums = {}
        for enum_dict in [self._load_mojo_enums(file) for file in mojo_files]:
            mojo_enums |= enum_dict

        js = "\n".join(f"export const {export_name} = "
                       f"{json.dumps(mojo_enums.get(import_name))};"
                       for import_name, export_name in exports)
        return js

    def _handle_color_css_updater_js(self, request_path: str) -> bytes:
        del request_path  # Unused.
        return (b"export const ColorChangeUpdater = "
                b"{forDocument: () => ({start: () => {}})};")

    def _handle_static_file(
        self,
        request_path: str,
        *,
        root: Optional[str] = None,
        path: Optional[Union[str, Callable[[str], str]]] = None,
        transform: Optional[Callable[[str, str], str]] = None,
    ) -> bytes:
        def calculate_path():
            if callable(path):
                return path(request_path)
            elif path is not None:
                return path
            else:
                return request_path.lstrip("/")

        root = root or self._cca_root
        path = calculate_path()

        with open(os.path.join(root, path), "rb") as f:
            content = f.read()
            if transform is not None:
                content = transform(request_path, content.decode()).encode()
            return content

    def _build_routes(self) -> List[_Route]:
        """
        Returns a list of routes served by the dev server.

        Note that bundle.py also use this same set of routes for generating
        static files bundle, so anything specific to dev server should be in
        DevServerHandler.
        """
        return [
            # Stubbed file from chrome://.
            _Route(
                "/chrome_stub/resources/cr_components/"
                "color_change_listener/colors_css_updater.js",
                self._handle_color_css_updater_js,
            ),
            _Route(
                "/chrome_stub/resources/js/load_time_data.js",
                functools.partial(
                    self._handle_static_file,
                    root=self._gen_dir,
                    path="ui/webui/resources/tsc/js/load_time_data.js",
                ),
            ),
            _Route(
                "/chrome_stub/resources/js/assert.js",
                functools.partial(
                    self._handle_static_file,
                    root=self._gen_dir,
                    path="ui/webui/resources/tsc/js/assert.js",
                ),
            ),
            _Route(
                re.compile("/chrome_stub/resources/(mwc|cros_components)/.*"),
                functools.partial(
                    self._handle_static_file,
                    root=os.path.join(self._gen_dir,
                                      "ui/webui/resources/tsc/"),
                    path=lambda path: "/".join(path.split("/")[3:]),
                    transform=_stub_chrome_url,
                ),
            ),
            _Route(
                "/chrome_stub/theme/typography.css",
                functools.partial(
                    self._handle_static_file,
                    root=self._dev_static_dir,
                    path="typography.css",
                ),
            ),
            _Route(
                "/chrome_stub/theme/colors.css",
                functools.partial(
                    self._handle_static_file,
                    root=self._dev_static_dir,
                    path="colors.css",
                ),
            ),
            # strings are generated dynamically from grd file.
            _Route("/strings.m.js", self._handle_strings_m_js),
            # All mojo imports are stubbed.
            _Route(
                "/js/mojo/type.js",
                functools.partial(
                    self._handle_static_file,
                    root=self._tsc_root,
                    transform=self._transform_js_mojo_js,
                ),
            ),
            # preload_images.js are dynamically generated from images.
            _Route("/js/preload_images.js", self._handle_preload_images_js),
            # local dev overrides are injected in init.js.
            _Route(
                "/js/init.js",
                functools.partial(
                    self._handle_static_file,
                    root=self._tsc_root,
                    transform=self._transform_init_js,
                ),
            ),
            # These two files are not compiled and need to be served from
            # self.cca_root.
            _Route("/js/lib/ffmpeg.js", self._handle_static_file),
            # All other .js files.
            _Route(
                re.compile(r"/.*\.js"),
                functools.partial(
                    self._handle_static_file,
                    root=self._tsc_root,
                    transform=self._transform_js,
                ),
            ),
            # All .html files
            _Route(
                re.compile(r"/.*\.html"),
                functools.partial(self._handle_static_file,
                                  transform=self._transform_html),
            ),
            # All .css files.
            _Route(
                re.compile(r"/.*\.css"),
                functools.partial(
                    self._handle_static_file,
                    transform=self._transform_css,
                ),
            ),
            # All other static files.
            _Route(re.compile(r"/.*"), self._handle_static_file),
        ]


class DevServerHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(
        self,
        handler: RequestHandler,
        *args,
        **kwargs,
    ):
        self._handler = handler

        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def _send_200(self, request_path: str, content: bytes):
        content_type = mimetypes.guess_type(request_path)[0]
        if content_type is None:
            raise RuntimeError(f"Can't guess MIME type for {request_path}.")

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def _handle_root(self):
        self.send_response(301)
        self.send_header("Location", "/views/main.html")
        self.end_headers()

    def do_GET(self):
        # Remove query parameters
        path = urllib_parse.urlparse(self.path).path

        if path == "/":
            return self._handle_root()

        def _route_match(route: _Route) -> bool:
            if isinstance(route.pattern, str):
                return path == route.pattern
            return route.pattern.fullmatch(path) is not None

        routes = self._handler.routes
        for route in routes:
            if _route_match(route):
                try:
                    content = route.handler(path)
                except Exception as e:
                    logging.debug("Error while handling %r: %r", path, e)
                    self.send_response(404)
                    self.end_headers()
                    return
                return self._send_200(path, content)

        self.send_response(404)
        self.end_headers()


_DEV_OUTPUT_TEMP_DIR = "/tmp/cca-dev-out"


@cli.command(
    "dev",
    help="run local dev server",
    description="run local dev server for UI development",
)
# TODO(pihsun): Should we derive the MWC tsconfig_library.json directly from
# BUILD.gn, so the board argument isn't needed?
@cli.option(
    "board",
    help=("board name. "
          "Use any board name with Chrome already built. "
          "The provided board name is used for finding MWC and lit, "
          "which is board independent. "
          "All other board dependent references will be stubbed."),
)
@cli.option(
    "--port",
    default=10224,
    type=int,
    help="server port",
)
def cmd(board: str, port: int) -> int:
    os.makedirs(_DEV_OUTPUT_TEMP_DIR, exist_ok=True)

    cca_root = os.getcwd()

    build.generate_tsconfig(board)

    # TODO(pihsun): Watch / live reload
    util.run_node([
        "typescript/bin/tsc",
        "--outDir",
        _DEV_OUTPUT_TEMP_DIR,
        "--noEmit",
        "false",
        # Makes compilation faster
        "--incremental",
        # For better debugging experience.
        "--inlineSourceMap",
        "--inlineSources",
        # Makes devtools show TypeScript source with better path
        "--sourceRoot",
        "/",
        # For easier developing / test cycle.
        "--noUnusedLocals",
        "false",
        "--noUnusedParameters",
        "false",
    ])

    handler = RequestHandler(cca_root, _DEV_OUTPUT_TEMP_DIR,
                             util.get_gen_dir(board))
    dev_server = http.server.ThreadingHTTPServer(
        ("localhost", port),
        lambda *args: DevServerHandler(handler, *args),
    )

    logging.info(f"Starting server on http://localhost:{port}")
    logging.info(
        "Attach a camera, or use utils/launch_dev_chrome.sh to launch "
        "Chrome with fake VCD. (You can port forward {port} "
        "and copy that script to your local machine "
        "if developing over ssh.)")
    dev_server.serve_forever()

    return 0
