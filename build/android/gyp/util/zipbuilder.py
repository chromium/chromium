# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python wrapper for signflinger.jar.

ZipBuilder creates a zip from a list of entries, and optionally signs it as an
APK (v2 + v4 signatures) in the same pass. See:
//third_party/android_build_tools/signflinger/
"""

import functools
import os
import platform
import sys
import tempfile

from util import build_utils

_SIGNFLINGER_DIR = os.path.join(
    build_utils.DIR_SOURCE_ROOT,
    'third_party',
    'android_build_tools',
    'signflinger',
    'cipd',
)
_SIGNFLINGER_JAR_PATH = os.path.join(_SIGNFLINGER_DIR, 'signflinger.jar')
_SIGNFLINGER_BINARY_PATH = os.path.join(_SIGNFLINGER_DIR, 'signflinger')
_MAIN_CLASS = 'org.chromium.build.ZipBuilder'

# CPU flags in /proc/cpuinfo required by the GraalVM native-image binary
# (-march=x86-64-v2 -H:CPUFeatures=AVX,AVX2,BMI2,SHA,CLMUL,AES). Flags that are
# part of the x86-64 baseline (cmov, cx8, fxsr, mmx, sse, sse2) are omitted.
_REQUIRED_CPU_FLAGS = frozenset(
    {
        # x86-64-v2.
        'pni',  # SSE3
        'popcnt',
        'sse4_1',
        'sse4_2',
        'ssse3',
        # -H:CPUFeatures.
        'aes',
        'avx',
        'avx2',
        'bmi2',
        'pclmulqdq',  # CLMUL
        'sha_ni',
    }
)

# Copies an entry from another zip without recompressing it.
KEEP_COMPRESSION = -1


@functools.cache
def _can_use_binary():
    if sys.platform != 'linux' or platform.machine() != 'x86_64':
        return False
    try:
        with open('/proc/cpuinfo', encoding='utf8') as f:
            for line in f:
                if line.startswith('flags') and ':' in line:
                    flags = set(line.split(':', 1)[1].split())
                    return _REQUIRED_CPU_FLAGS.issubset(flags)
    except OSError:
        pass
    return False


def get_input_paths():
    """Returns the tool paths read by ZipBuilder.write()."""
    if _can_use_binary():
        return [_SIGNFLINGER_BINARY_PATH]
    return [build_utils.JAVA_PATH_FOR_INPUTS, _SIGNFLINGER_JAR_PATH]


class ZipBuilder:
    """Collects entries and then runs ZipBuilder to write them to a zip.

    Entries are written in the order they are added. Must be used as a context
    manager so any temporary files created by add_data() are cleaned up.
    """

    def __init__(self):
        self._tmp_dir = None
        self._lines = []
        self._names = set()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        if self._tmp_dir:
            self._tmp_dir.cleanup()
            self._tmp_dir = None

    @staticmethod
    def _check_posix_relpath(path):
        assert '\\' not in path, f'Non-posix path: {path}'
        assert not os.path.isabs(path), f'Absolute path: {path}'
        assert '..' not in path.split('/'), f'Relative path: {path}'

    def _append_fields(self, zip_path, compress_level, alignment, *rest):
        fields = (zip_path, str(compress_level), str(alignment or 0), *rest)
        for field in fields:
            assert '\t' not in field and '\n' not in field, fields
        self._lines.append('\t'.join(fields))

    def _add_line(self, zip_path, compress_level, alignment, *rest):
        self._check_posix_relpath(zip_path)
        assert zip_path == os.path.normpath(zip_path), (
            f'Non-canonical path: {zip_path}'
        )
        if zip_path in self._names:
            raise Exception(f'Duplicate zip entry: {zip_path}')
        self._names.add(zip_path)
        self._append_fields(zip_path, compress_level, alignment, *rest)

    def add_file(self, zip_path, src_path, *, compress_level, alignment=None):
        """Adds a file from disk. compress_level=0 stores it uncompressed."""
        self._add_line(zip_path, compress_level, alignment, src_path)

    def add_data(self, zip_path, data, *, compress_level, alignment=None):
        """Adds an entry with the given bytes (or str) as contents."""
        if isinstance(data, str):
            data = data.encode('utf8')
        if not self._tmp_dir:
            self._tmp_dir = tempfile.TemporaryDirectory()
        src_path = os.path.join(self._tmp_dir.name, str(len(self._lines)))
        with open(src_path, 'wb') as f:
            f.write(data)
        self.add_file(
            zip_path,
            src_path,
            compress_level=compress_level,
            alignment=alignment,
        )

    def add_from_zip(
        self,
        zip_path,
        src_zip_path,
        src_entry_name,
        *,
        compress_level=KEEP_COMPRESSION,
        alignment=None,
    ):
        """Copies an entry from another zip (without recompressing by default).

        Pass compress_level=KEEP_COMPRESSION (the default) to copy the entry's
        bytes as-is; other levels recompress (0 = store).
        """
        assert src_entry_name
        self._add_line(
            zip_path, compress_level, alignment, src_zip_path, src_entry_name
        )

    def add_all_from_zip(
        self,
        src_zip_path,
        *,
        dst_prefix='',
        compress_level=KEEP_COMPRESSION,
        alignment=None,
    ):
        """Copies all remaining non-directory entries from src_zip_path.

        Entries already copied from src_zip_path by an earlier add_from_zip()
        call are skipped; the rest are added in sorted order with dst_prefix
        prepended. alignment is applied only to uncompressed entries.
        """
        if dst_prefix:
            assert dst_prefix.endswith('/'), dst_prefix
            self._check_posix_relpath(dst_prefix)
        self._append_fields(
            dst_prefix, compress_level, alignment, src_zip_path, ''
        )

    def write(
        self,
        output_path,
        *,
        key_path=None,
        key_passwd=None,
        key_name=None,
        min_sdk_version=None,
        warnings_as_errors=False,
    ):
        """Writes the zip. Signs it as an APK when key_path is given.

        When signing, the v4 signature is written to <output_path>.idsig.
        """
        with tempfile.NamedTemporaryFile(
            'w', encoding='utf8', suffix='.txt'
        ) as spec_file:
            spec_file.write('\n'.join(self._lines))
            spec_file.write('\n')
            spec_file.flush()

            if _can_use_binary():
                cmd = [_SIGNFLINGER_BINARY_PATH, '-Xmx1G']
            else:
                cmd = build_utils.JavaCmd() + [
                    '-cp',
                    _SIGNFLINGER_JAR_PATH,
                    _MAIN_CLASS,
                ]
            cmd += [
                '--output',
                output_path,
                '--spec',
                spec_file.name,
            ]
            if key_path:
                # Use SHA-1 for v1 signing (faster; insecure, but local-only).
                v1_signing = min_sdk_version < 24
                effective_min_sdk = 1 if v1_signing else min_sdk_version
                cmd += [
                    '--keystore',
                    key_path,
                    '--key-alias',
                    key_name,
                    '--key-password',
                    key_passwd,
                    '--min-sdk-version',
                    str(effective_min_sdk),
                    '--v1-signing-enabled',
                    str(v1_signing).lower(),
                    # Note: v2 signing cannot be disabled even when
                    # minSdkVersion >= 30 while v4 signing is enabled without
                    # v3, because v4 (.idsig) signs the APK Signing Block digest
                    # produced by v2/v3 (V4SchemeSigner.getApkDigests).
                    '--v2-signing-enabled',
                    'true',
                    '--v3-signing-enabled',
                    'false',
                    '--v4-signing-enabled',
                    'true',
                ]
            build_utils.CheckOutput(
                cmd, print_stdout=True, fail_on_output=warnings_as_errors
            )
