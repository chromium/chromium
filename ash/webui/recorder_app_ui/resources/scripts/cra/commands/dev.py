# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import http.server
import json
import logging
import mimetypes
import os
import pathlib
import re
from typing import Callable, NamedTuple, Optional, Union
from urllib import parse as urllib_parse
from xml.dom import minidom

from cra import build
from cra import cli
from cra import util

_RequestPath = pathlib.PurePosixPath


def _get_root_relative_path(request_path: _RequestPath) -> _RequestPath:
    request_path = request_path.parent
    root = _RequestPath("/")
    # Note that relative_to can't be used here since it requires the path to be
    # inside target folder.
    return _RequestPath(os.path.relpath(root, root / request_path))


def _stub_chrome_url(request_path: _RequestPath, s: str) -> str:
    """
    Replaces all chrome:// reference to /chrome_stub/.

    Also replaces all //resources/ references to /chrome_stub/resources/, since
    some imports in cros_component and mwc use // instead of chrome://, but
    replacing all '//' is too broad.
    """
    # Note that the path join needs to be done via string interpolation instead
    # of manipulation on _RequestPath, since './chrome_stub' gets transformed
    # into 'chrome_stub' by pathlib.
    chrome_stub_path = f"{_get_root_relative_path(request_path)}/chrome_stub/"
    return s.replace("chrome://", chrome_stub_path).replace(
        "//resources/", f"{chrome_stub_path}resources/")


class _Route(NamedTuple):
    # The url pattern for the route. Can be a regex.
    pattern: Union[_RequestPath, re.Pattern]
    # Handler of the route, takes path as argument and returns response in
    # bytes and the content type.
    handler: Callable[[_RequestPath], tuple[bytes, str]]


# importmap tag to make the import of /images/images.js independent of the base
# path. The import is absolute because of build issue (See comment in
# resources/BUILD.gn on ts_path_mappings).
IMPORT_MAP = ('<script type="importmap">' +
              '{"imports":{"/images/": "./images/"}}' + '</script>')


class RequestHandler:

    def __init__(
        self,
        cra_root: pathlib.Path,
        tsc_root: pathlib.Path,
        build_dir: pathlib.Path,
        strings_dir: pathlib.Path,
    ):
        self._cra_root = cra_root
        self._tsc_root = tsc_root
        self._gen_dir = build_dir / "gen"
        self._strings_dir = strings_dir
        self._dev_static_dir = self._cra_root / "scripts/cra/static"
        self._routes = self._build_routes()
        mimetypes.add_type('application/javascript', '.map')

    def _transform_html(self, request_path: _RequestPath, html: str) -> str:
        html = _stub_chrome_url(request_path, html)

        relative_path = _get_root_relative_path(request_path)
        html = re.sub(r"(href|src)=\"/", f'\\1="{relative_path}/', html)
        # Put the import map before the first script tag.
        html = html.replace('<script', IMPORT_MAP + '<script')

        strings = self._load_grd_strings()

        def i18n_replace(m: re.Match):
            return strings.get(m.group(1), '')

        # Replace the i18n title that is used.
        html = re.sub(r'\$i18n\{(\w+)\}', i18n_replace, html)

        return html

    def _transform_js(self, request_path: _RequestPath, js: str) -> str:
        return _stub_chrome_url(request_path, js)

    def _transform_platforms_index_js(self, request_path: _RequestPath,
                                      js: str) -> str:
        # TODO(pihsun): The inline source would still be wrong, have some hacky
        # way to fix that too.
        js = js.replace("'./swa/handler.js'", "'./dev/handler.js'")
        return self._transform_js(request_path, js)

    def _load_grd_strings(self) -> dict[str, str]:

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
        grd_path = self._strings_dir / "recorder_strings.grdp"
        dom = minidom.parse(str(grd_path))
        messages = dom.getElementsByTagName("grit-part")[0]
        for message in messages.getElementsByTagName("message"):
            name = message.getAttribute("name")
            value = get_message_text_content(message).strip()
            assert name.startswith("IDS_RECORDER_")
            id = name[len("IDS_RECORDER_"):]
            id = util.to_camel_case(id)
            strings[id] = value
        return strings

    def _handle_dev_strings_js(
            self, _request_path: _RequestPath) -> tuple[bytes, str]:
        grd_strings = self._load_grd_strings()

        return (f"export const strings = {json.dumps(grd_strings)};".encode(),
                "text/javascript")

    def _handle_images_js(self,
                          request_path: _RequestPath) -> tuple[bytes, str]:
        # TODO(pihsun): With watch, we can cache the result and only
        # re-generate when any image files are changed.
        return (self._transform_js(request_path,
                                   build.gen_images_js()).encode(),
                "text/javascript")

    def _handle_static_file(
        self,
        request_path: _RequestPath,
        *,
        root: Optional[pathlib.Path] = None,
        path: Optional[Union[_RequestPath, Callable[[_RequestPath],
                                                    _RequestPath]]] = None,
        transform: Optional[Callable[[_RequestPath, str], str]] = None,
        content_type: Optional[str] = None,
    ) -> tuple[bytes, str]:

        def calculate_path():
            if callable(path):
                return path(request_path)
            return path or request_path

        root = root or self._cra_root
        path = calculate_path()

        if content_type is None:
            content_type = mimetypes.guess_type(path)[0]
            if content_type is None:
                raise RuntimeError(
                    f"Can't guess MIME type for {request_path} ({path}).")

        with open(root / path, "rb") as f:
            content = f.read()
            if transform is not None:
                content = transform(request_path, content.decode()).encode()
            return (content, content_type)

    def _build_routes(self) -> list[_Route]:
        """
        Returns a list of routes served by the dev server.

        Note that bundle.py also use this same set of routes for generating
        static files bundle, so anything specific to dev server should be in
        DevServerHandler.
        """
        return [
            # Stubbed file from chrome://.
            _Route(
                re.compile("chrome_stub/resources/(mwc|cros_components)/.*"),
                functools.partial(
                    self._handle_static_file,
                    root=self._gen_dir / "ui/webui/resources/tsc/",
                    path=lambda path: _RequestPath(*path.parts[2:]),
                    transform=_stub_chrome_url,
                ),
            ),
            # Stubbed css files.
            _Route(
                _RequestPath("chrome_stub/theme/typography.css"),
                functools.partial(
                    self._handle_static_file,
                    root=self._dev_static_dir,
                    path=_RequestPath("typography.css"),
                ),
            ),
            _Route(
                _RequestPath("chrome_stub/theme/colors.css"),
                functools.partial(
                    self._handle_static_file,
                    root=self._dev_static_dir,
                    path=_RequestPath("colors.css"),
                ),
            ),
            # All static files.
            _Route(re.compile(r"static/.*"), self._handle_static_file),
            # images.js are dynamically generated from images.
            _Route(_RequestPath("images/images.js"), self._handle_images_js),
            # platforms/index.js needs special transform to change the platform
            # used.
            _Route(
                _RequestPath("platforms/index.js"),
                functools.partial(
                    self._handle_static_file,
                    root=self._tsc_root,
                    transform=self._transform_platforms_index_js,
                ),
            ),
            # platforms/dev/strings.js is for strings in dev server.
            _Route(
                _RequestPath("platforms/dev/strings.js"),
                self._handle_dev_strings_js,
            ),
            # All other .js files.
            _Route(
                re.compile(r".*\.js"),
                functools.partial(
                    self._handle_static_file,
                    root=self._tsc_root,
                    transform=self._transform_js,
                ),
            ),
            # index.html.
            _Route(
                _RequestPath("index.html"),
                functools.partial(self._handle_static_file,
                                  transform=self._transform_html),
            ),
            # Other request path without extension, assuming that it's handled
            # by client side navigation.
            # Files with extension is omitted so it's easier to debug when
            # import paths are wrong.
            # Note that "." is also included since empty relative path is
            # represented by "." by pathlib.
            _Route(
                re.compile(r"[^.]*|\."),
                functools.partial(self._handle_static_file,
                                  path=_RequestPath("index.html"),
                                  transform=self._transform_html),
            ),
        ]

    def handle(self, path: _RequestPath) -> Optional[tuple[bytes, str]]:

        def _route_match(route: _Route) -> bool:
            if isinstance(route.pattern, _RequestPath):
                return path == route.pattern
            return route.pattern.fullmatch(str(path)) is not None

        for route in self._routes:
            if _route_match(route):
                return route.handler(path)

        return None


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

    def _send_200(self, content: bytes, content_type: str):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def do_GET(self):
        # Remove query parameters, and transform to relative path.
        path = _RequestPath(urllib_parse.urlparse(self.path).path).relative_to(
            _RequestPath("/"))

        try:
            resp = self._handler.handle(path)
        except Exception as e:
            logging.debug("Error while handling %r: %r",
                          path,
                          e,
                          exc_info=True)
            self.send_response(404)
            self.end_headers()
            return

        if resp is None:
            self.send_response(404)
            self.end_headers()
            return

        content, content_type = resp
        return self._send_200(content, content_type)


_DEV_OUTPUT_TEMP_DIR = pathlib.Path("/tmp/cra-dev-out")


@cli.command(
    "dev",
    help="run local dev server",
    description="run local dev server for UI development",
)
@util.build_dir_option()
@cli.option(
    "--port",
    default=10244,
    type=int,
    help="server port",
)
def cmd(build_dir: pathlib.Path, port: int) -> int:
    _DEV_OUTPUT_TEMP_DIR.mkdir(parents=True, exist_ok=True)

    build.generate_tsconfig(build_dir)

    # TODO(pihsun): Watch / live reload
    util.run_node(
        [
            "typescript/bin/tsc",
            "--outDir",
            str(_DEV_OUTPUT_TEMP_DIR),
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
        ],
        cwd=util.get_cra_root())

    handler = RequestHandler(util.get_cra_root(), _DEV_OUTPUT_TEMP_DIR,
                             build_dir, util.get_strings_dir())
    dev_server = http.server.ThreadingHTTPServer(
        ("localhost", port),
        lambda *args: DevServerHandler(handler, *args),
    )

    logging.info(f"Starting server on http://localhost:{port}")
    dev_server.serve_forever()

    return 0
