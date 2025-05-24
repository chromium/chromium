# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Executes a browser with devtools enabled on the target."""

import os
import re
import subprocess
import tempfile
import time
from typing import List, Optional
from urllib.parse import urlparse

from common import run_continuous_ffx_command, ssh_run, REPO_ALIAS
from ffx_integration import run_symbolizer

WEB_ENGINE_SHELL = 'web-engine-shell'
CAST_STREAMING_SHELL = 'cast-streaming-shell'


class BrowserRunner:
    """Manages the browser process on the target."""

    def __init__(self,
                 browser_type: str,
                 target_id: Optional[str] = None,
                 output_dir: Optional[str] = None):
        self._browser_type = browser_type
        assert self._browser_type in [WEB_ENGINE_SHELL, CAST_STREAMING_SHELL]
        self._target_id = target_id
        self._output_dir = output_dir or os.environ['CHROMIUM_OUTPUT_DIR']
        assert self._output_dir
        self._browser_proc = None
        self._symbolizer_proc = None
        self._devtools_port = None
        self._log_fs = None

        output_root = os.path.join(self._output_dir, 'gen', 'fuchsia_web')
        if self._browser_type == WEB_ENGINE_SHELL:
            self._id_files = [
                os.path.join(output_root, 'shell', 'web_engine_shell',
                             'ids.txt'),
                os.path.join(output_root, 'webengine', 'web_engine_with_webui',
                             'ids.txt'),
            ]
        else:  # self._browser_type == CAST_STREAMING_SHELL:
            self._id_files = [
                os.path.join(output_root, 'shell', 'cast_streaming_shell',
                             'ids.txt'),
                os.path.join(output_root, 'webengine', 'web_engine',
                             'ids.txt'),
            ]

    @property
    def browser_type(self) -> str:
        """Returns the type of the browser for the tests."""
        return self._browser_type

    @property
    def devtools_port(self) -> int:
        """Returns the randomly assigned devtools-port, shouldn't be called
        before executing the start."""
        assert self._devtools_port
        return self._devtools_port

    @property
    def log_file(self) -> str:
        """Returns the log file of the browser instance, shouldn't be called
        before executing the start."""
        assert self._log_fs
        return self._log_fs.name

    @property
    def browser_pid(self) -> int:
        """Returns the process id of the ffx instance which starts the browser
        on the test device, shouldn't be called before executing the start."""
        assert self._browser_proc
        return self._browser_proc.pid

    def _read_devtools_port(self):
        search_regex = r'DevTools listening on (.+)'

        # The ipaddress of the emulator or device is preferred over the address
        # reported by the devtools, former one is usually more accurate.
        def try_reading_port(log_file) -> int:
            for line in log_file:
                tokens = re.search(search_regex, line)
                if tokens:
                    url = urlparse(tokens.group(1))
                    assert url.scheme == 'ws'
                    assert url.port is not None
                    return url.port
            return None

        with open(self.log_file, encoding='utf-8') as log_file:
            start = time.time()
            while time.time() - start < 180:
                port = try_reading_port(log_file)
                if port:
                    return port
                self._browser_proc.poll()
                assert not self._browser_proc.returncode, 'Browser stopped.'
                time.sleep(1)
            assert False, 'Failed to wait for the devtools port.'

    def start(self, extra_args: List[str] = None) -> None:
        """Starts the selected browser, |extra_args| are attached to the command
        line."""
        browser_cmd = ['test', 'run']
        if self.browser_type == WEB_ENGINE_SHELL:
            browser_cmd.extend([
                f'fuchsia-pkg://{REPO_ALIAS}/web_engine_shell#meta/'
                f'web_engine_shell.cm',
                '--',
                '--web-engine-package-name=web_engine_with_webui',
                '--remote-debugging-port=0',
                '--enable-web-instance-tmp',
                '--with-webui',
                'about:blank',
            ])
        else:  # if self.browser_type == CAST_STREAMING_SHELL:
            browser_cmd.extend([
                f'fuchsia-pkg://{REPO_ALIAS}/cast_streaming_shell#meta/'
                f'cast_streaming_shell.cm',
                '--',
                '--remote-debugging-port=0',
            ])
        # Use flags used on WebEngine in production devices.
        browser_cmd.extend([
            '--',
            '--enable-low-end-device-mode',
            '--force-gpu-mem-available-mb=64',
            '--force-gpu-mem-discardable-limit-mb=32',
            '--force-max-texture-size=2048',
            '--gpu-rasterization-msaa-sample-count=0',
            '--min-height-for-gpu-raster-tile=128',
            '--webgl-msaa-sample-count=0',
            '--max-decoded-image-size-mb=10',
        ])
        if extra_args:
            browser_cmd.extend(extra_args)
        self._browser_proc = run_continuous_ffx_command(
            cmd=browser_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            target_id=self._target_id)
        # The stdout will be forwarded to the symbolizer, then to the _log_fs.
        self._log_fs = tempfile.NamedTemporaryFile()
        self._symbolizer_proc = run_symbolizer(self._id_files,
                                               self._browser_proc.stdout,
                                               self._log_fs)
        self._devtools_port = self._read_devtools_port()

    def stop_browser(self) -> None:
        """Stops the browser on the target, as well as the local symbolizer, the
        _log_fs is preserved. Calling this function for a second time won't have
        any effect."""
        if not self.is_browser_running():
            return
        self._browser_proc.kill()
        self._browser_proc = None
        self._symbolizer_proc.kill()
        self._symbolizer_proc = None
        self._devtools_port = None
        # The process may be stopped already, ignoring the no process found
        # error.
        ssh_run(['killall', 'web_instance.cmx'], self._target_id, check=False)

    def is_browser_running(self) -> bool:
        """Checks if the browser is still running."""
        if self._browser_proc:
            assert self._symbolizer_proc
            assert self._devtools_port
            return True
        assert not self._symbolizer_proc
        assert not self._devtools_port
        return False

    def close(self) -> None:
        """Cleans up everything."""
        self.stop_browser()
        self._log_fs.close()
        self._log_fs = None
