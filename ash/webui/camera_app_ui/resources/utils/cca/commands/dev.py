# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import http.server
import json
import logging
import mimetypes
import os
import re
from typing import Callable, Dict, Optional, NamedTuple
from xml.dom import minidom

from cca import build
from cca import cli
from cca import util


class PathHandler(NamedTuple):
    # Root of the target file. Default to cca_root.
    root: Optional[str] = None
    # Relative path of the target file from root. Default to self.path with
    # leading '/' removed.
    path: Optional[str] = None
    # Transformation to be applied to the target file content. Default to no
    # transformation.
    transform: Optional[Callable[[str], str]] = None


class DevServerHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(
        self,
        cca_root: str,
        tsc_root: str,
        gen_dir: str,
        *args,
        **kwargs,
    ):
        self.cca_root = cca_root
        self.tsc_root = tsc_root
        self.gen_dir = gen_dir
        self.directory = self.cca_root

        self.path_mapping: Dict[str, PathHandler] = {
            "/views/main.html":
            PathHandler(transform=self._transform_main_html),
            "/js/mojo/type.js":
            PathHandler(root=tsc_root, transform=self._transform_js_mojo_js),
            "/chrome_stub/resources/mwc/lit/index.js":
            PathHandler(
                root=gen_dir,
                path="ui/webui/resources/tsc/mwc/lit/index.js",
            ),
            "/chrome_stub/resources/js/load_time_data.js":
            PathHandler(
                root=gen_dir,
                path="ui/webui/resources/tsc/js/load_time_data.js",
            ),
            "/chrome_stub/resources/js/assert_ts.js":
            PathHandler(
                root=gen_dir,
                path="ui/webui/resources/tsc/js/assert_ts.js",
            ),
        }

        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def _send_200(self, content: str, *, content_type: Optional[str] = None):
        if content_type is None:
            content_type = mimetypes.guess_type(self.path)[0]
            if content_type is None:
                raise RuntimeError(
                    f"Can't guess MIME type for {self.path}, please specify it."
                )

        content_bytes = content.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content_bytes)))
        self.end_headers()
        self.wfile.write(content_bytes)

    def _load_grd_strings(self) -> Dict[str, str]:
        def get_message_text_content(message: minidom.Element) -> str:
            pieces = []
            for child in message.childNodes:
                if child.nodeType == minidom.Element.TEXT_NODE:
                    pieces.append(child.nodeValue)
                if child.nodeType == minidom.Element.ELEMENT_NODE:
                    if child.tagName == 'ex':
                        continue
                    pieces.append(get_message_text_content(child))
            return "".join(pieces)

        strings = {}
        grd_path = os.path.join(self.cca_root, "strings/camera_strings.grd")
        dom = minidom.parse(grd_path)
        messages = dom.getElementsByTagName("messages")[0]
        for message in messages.getElementsByTagName("message"):
            name = message.getAttribute("name").lower()
            value = get_message_text_content(message).strip()
            assert name.startswith("ids_")
            id = name[len("ids_"):]
            strings[id] = value
        return strings

    def _handle_strings_m_js(self):
        load_time_data = {
            "board_name": "local-dev",
            "browser_version": "unknown",
            "device_type": "unknown",
            "is_test_image": True,
            # TODO(pihsun): After jelly is enabled by default and
            # colors_default.css is removed, we need to inject a static copy of
            # the colors / typography css.
            "jelly": False,
            "textdirection": "ltr",
            "timeLapse": True,
        }
        load_time_data.update(self._load_grd_strings())

        self._send_200("import {loadTimeData} from "
                       "'/chrome_stub/resources/js/load_time_data.js';"
                       f"loadTimeData.data = {json.dumps(load_time_data)}")

    def _transform_main_html(self, html: str) -> str:
        name = self._load_grd_strings()["name"]

        html = html.replace("$i18n{name}", name)
        html = html.replace("chrome://", "/chrome_stub/")
        return html

    def _transform_js(self, js: str) -> str:
        return js.replace("chrome://", "/chrome_stub/")

    def _load_camera_app_helper_mojo_enums(self) -> Dict[str, Dict[str, int]]:
        with open(os.path.join(self.cca_root, "../camera_app_helper.mojom"),
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

    def _transform_js_mojo_js(self, ts_src: str) -> str:
        export_blocks = re.findall(r"export \{(.*?)\}", ts_src, re.DOTALL)
        exports = [
            export.strip() for block in export_blocks
            for export in block.split(",")
        ]
        # Ignore empty value from split.
        exports = [export for export in exports if export]
        # Some exports are in form export {A as B}, take the last `B` part in
        # this case.
        exports = [export.split()[-1] for export in exports if export]

        # Stub the real enum values for enum in camera_app_helper.mojom, since
        # those enum values are used by CCA.
        camera_app_helper_mojo_enums = (
            self._load_camera_app_helper_mojo_enums())

        js = "\n".join(
            f"export const {export} = "
            f"{json.dumps(camera_app_helper_mojo_enums.get(export))};"
            for export in exports)
        return js

    def _run_handler(self, handler: PathHandler):
        root = handler.root or self.cca_root
        path = handler.path or self.path[1:]
        transform = handler.transform or (lambda s: s)
        with open(os.path.join(root, path), "r") as f:
            self._send_200(transform(f.read()))

    def do_GET(self):
        path = self.path

        handler = self.path_mapping.get(path)
        if handler is not None:
            return self._run_handler(handler)

        if path == "/":
            self.send_response(301)
            self.send_header("Location", "/views/main.html")
            self.end_headers()
            return

        if path == "/strings.m.js":
            return self._handle_strings_m_js()

        if path == "/chrome_stub/theme/typography.css":
            return self._send_200("")

        if path == ("/chrome_stub/resources/cr_components/"
                    "color_change_listener/colors_css_updater.js"):
            # This is not actually use since we returns jelly = false.
            return self._send_200("export const ColorChangeUpdater = null;")

        if path.startswith("/js/"):
            return self._run_handler(
                PathHandler(root=self.tsc_root, transform=self._transform_js))

        return super().do_GET()


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
    js_out_dir = os.path.join(_DEV_OUTPUT_TEMP_DIR, "js")

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

    # TODO(pihsun): Watch images and rebuild
    build.build_preload_images_js(js_out_dir)

    dev_server = http.server.ThreadingHTTPServer(
        ("localhost", port),
        lambda *args: DevServerHandler(cca_root, _DEV_OUTPUT_TEMP_DIR,
                                       util.get_gen_dir(board), *args),
    )

    # TODO(pihsun): Mention that chrome needs
    # --use-fake-device-for-media-stream=fps=30 (also --user-data-dir=)
    # (also the parameter that skips asking camera permission dialog, see
    # factory repo.)
    logging.info(f"Starting server on http://localhost:{port}")
    logging.info(
        "Attach a camera, or use utils/launch_dev_chrome.sh to launch "
        "Chrome with fake VCD. (You can port forward {port} "
        "and copy that script to your local machine "
        "if developing over ssh.)")
    dev_server.serve_forever()

    return 0
