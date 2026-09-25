# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for building GraalVM native-image binaries in 3pp packages."""

import json
import logging
import os
import pathlib
import shutil
import tempfile
import zipfile

import common
import fetch_github_release

_GRAALVM_PROJECT = 'graalvm/graalvm-ce-builds'
_GRAALVM_ARTIFACT_REGEX = r'graalvm-community-jdk-.*_linux-x64_bin\.tar\.gz$'

# Without this wrapper, JAVA_HOME is not respected. The wrapper could be
# omitted if -Djava.home= is always set via command-line.
# Defaults to "." when JAVA_HOME is unset to satisfy non-null java.home checks
# (e.g., javac Locations.<clinit> in google-java-format) when no JDK files are
# read from disk.
_WRAPPER_TEMPLATE = """\
public class GraalVmWrapper {{
  public static void main(String[] args) throws Exception {{
    if (System.getProperty("java.home") == null) {{
      String javaHome = System.getenv("JAVA_HOME");
      System.setProperty("java.home", javaHome != null ? javaHome : ".");
    }}
    {main_class}.main(args);
  }}
}}
"""


def _find_native_image(root):
    if os.path.isdir(root):
        for child in os.listdir(root):
            candidate = os.path.join(root, child)
            if os.path.exists(os.path.join(candidate, 'bin', 'native-image')):
                return candidate
    return None


def ensure_graalvm():
    """Downloads and extracts GraalVM if needed, returning its root path."""
    # This hook allows providing a local graalvm for local testing.
    env_home = os.environ.get('GRAALVM_HOME')
    if env_home and os.path.exists(
        os.path.join(env_home, 'bin', 'native-image')
    ):
        logging.info('Using GRAALVM_HOME=%s', env_home)
        return env_home

    extract_root = os.path.abspath('.graalvm')
    found = _find_native_image(extract_root)
    if found:
        return found

    version = fetch_github_release.latest_release(
        _GRAALVM_PROJECT, artifact_regex=_GRAALVM_ARTIFACT_REGEX
    )
    url = fetch_github_release.get_release_url(
        _GRAALVM_PROJECT, version, artifact_regex=_GRAALVM_ARTIFACT_REGEX
    )
    filename = url.rsplit('/', 1)[-1]
    cache_tar = os.path.join(tempfile.gettempdir(), filename)
    if not os.path.exists(cache_tar):
        tmp_tar = f'{cache_tar}.tmp.{os.getpid()}'
        common.download_file(url, tmp_tar)
        os.replace(tmp_tar, cache_tar)

    os.makedirs(extract_root, exist_ok=True)
    common.extract_tar(cache_tar, extract_root)
    found = _find_native_image(extract_root)
    if found:
        return found

    raise RuntimeError(
        f'Failed to locate bin/native-image under {extract_root}'
    )


def _parse_manifest(jar_path):
    """Extracts (Main-Class, Add-Exports list) from a jar's MANIFEST.MF."""
    main_class = None
    add_exports = []
    with zipfile.ZipFile(jar_path) as z:
        if 'META-INF/MANIFEST.MF' not in z.namelist():
            return main_class, add_exports
        raw = z.read('META-INF/MANIFEST.MF').decode('utf-8', errors='replace')
    # Unwrap RFC 822 continuation lines (newline followed by single space).
    unwrapped = raw.replace('\r\n ', '').replace('\n ', '')
    for line in unwrapped.splitlines():
        if ':' not in line:
            continue
        key, val = line.split(':', 1)
        key = key.strip()
        val = val.strip()
        if key == 'Main-Class':
            main_class = val
        elif key == 'Add-Exports':
            add_exports.extend(val.split())
    return main_class, add_exports


def _merge_reachability_metadata(config_dir, extra_metadata):
    metadata_path = os.path.join(config_dir, 'reachability-metadata.json')
    if os.path.exists(metadata_path):
        with open(metadata_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    else:
        data = {}

    for section in ('reflection', 'resources', 'jni', 'serialization'):
        items = extra_metadata.get(section)
        if items:
            data.setdefault(section, []).extend(items)

    with open(metadata_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)


def _format_export(exp):
    return exp if '=' in exp else f'{exp}=ALL-UNNAMED'


def build_native_image(
    jar_path,
    output_path,
    *,
    main_class=None,
    extra_cp=None,
    add_exports=None,
    tracing_args_list=None,
    extra_reachability_metadata=None,
    extra_native_image_args=None,
):
    """Builds a GraalVM native-image binary from a jar file.

    Args:
      jar_path: Path to the main .jar file.
      output_path: Destination path for the compiled executable.
      main_class: Main class name. If None, read from MANIFEST.MF.
      extra_cp: Optional list of .jar paths to prepend to the classpath.
      add_exports: Optional list of module/package strings for --add-exports.
      tracing_args_list: Optional list of argv lists to run under the GraalVM
          tracing agent (-agentlib:native-image-agent) before compiling.
      extra_reachability_metadata: Optional dict with 'reflection' and/or
          'resources' lists to merge into reachability-metadata.json.
      extra_native_image_args: Optional list of extra flags for native-image.
    """
    graal_home = ensure_graalvm()
    java_bin = os.path.join(graal_home, 'bin', 'java')
    javac_bin = os.path.join(graal_home, 'bin', 'javac')
    native_image_bin = os.path.join(graal_home, 'bin', 'native-image')

    jar_path = os.path.abspath(jar_path)
    output_path = os.path.abspath(output_path)

    manifest_main, manifest_exports = _parse_manifest(jar_path)
    if not main_class:
        main_class = manifest_main
    if not main_class:
        raise ValueError(f'No main_class specified or found in {jar_path}')

    all_exports = []
    for exp in manifest_exports + (add_exports or []):
        formatted = _format_export(exp)
        if formatted not in all_exports:
            all_exports.append(formatted)

    cp_entries = [os.path.abspath(p) for p in (extra_cp or [])] + [jar_path]
    classpath = os.pathsep.join(cp_entries)

    with tempfile.TemporaryDirectory(
        prefix='graalvm_build_', dir='.'
    ) as work_dir:
        config_dir = os.path.abspath(os.path.join(work_dir, 'config'))
        os.makedirs(config_dir, exist_ok=True)

        # 1. Run the tracing agent if tracing commands are provided.
        for tracing_args in tracing_args_list or []:
            agent_flag = (
                f'-agentlib:native-image-agent=config-merge-dir={config_dir}'
            )
            agent_cmd = [java_bin, agent_flag]
            for exp in all_exports:
                agent_cmd.append(f'--add-exports={exp}')
            agent_cmd.extend(['-cp', classpath, main_class])
            agent_cmd.extend(tracing_args)
            # Tracing runs may intentionally exit non-zero (e.g. checkstyle).
            common.run_cmd(agent_cmd, check=False)

        if tracing_args_list and not os.listdir(config_dir):
            raise RuntimeError(
                f'Tracing agent failed to generate config in {config_dir}'
            )

        if extra_reachability_metadata:
            _merge_reachability_metadata(
                config_dir, extra_reachability_metadata
            )

        # 2. Compile GraalVmWrapper so java.home is initialized at startup.
        wrapper_dir = os.path.join(work_dir, 'wrapper')
        os.makedirs(wrapper_dir, exist_ok=True)
        wrapper_java = os.path.join(wrapper_dir, 'GraalVmWrapper.java')
        pathlib.Path(wrapper_java).write_text(
            _WRAPPER_TEMPLATE.format(main_class=main_class), encoding='utf-8'
        )

        javac_cmd = [javac_bin]
        for exp in all_exports:
            javac_cmd.append(f'--add-exports={exp}')
        javac_cmd.extend(['-cp', classpath, '-d', wrapper_dir, wrapper_java])
        common.run_cmd(javac_cmd)

        # 3. Build the native image into work_dir, then copy only the executable
        # to output_path so any unused auxiliary JDK .so files (libjsound.so)
        # do not pollute output_prefix.
        temp_output = os.path.abspath(
            os.path.join(work_dir, os.path.basename(output_path))
        )
        full_cp = os.pathsep.join([os.path.abspath(wrapper_dir)] + cp_entries)
        ni_cmd = [
            native_image_bin,
            '-cp',
            full_cp,
            f'-H:ConfigurationFileDirectories={config_dir}',
            '-o',
            temp_output,
        ]
        for exp in all_exports:
            ni_cmd.append(f'-J--add-exports={exp}')
        if extra_native_image_args:
            ni_cmd.extend(extra_native_image_args)
        ni_cmd.append('GraalVmWrapper')
        common.run_cmd(ni_cmd)
        shutil.copy(temp_output, output_path)
