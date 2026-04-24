#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Given a .build_config.json file, generates an Eclipse JDT project that can
be used with the "Language Support for Java™ by Red Hat" Visual Studio Code
extension. See //docs/vscode.md for details.
"""

import argparse
import logging
import json
import os
import re
import shutil
import sys
import xml.etree.ElementTree
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import build_utils

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
import gn_helpers


def _GetJavaReleaseVersion():
  """Read the --release value from compile_java.py."""
  compile_java_path = os.path.join(os.path.dirname(__file__), 'gyp',
                                   'compile_java.py')
  with open(compile_java_path) as f:
    contents = f.read()
  m = re.search(r"'--release',\s*'(\d+)'", contents)
  if m:
    return m.group(1)
  return '17'


def _WithoutSuffix(string, suffix):
  if not string.endswith(suffix):
    raise ValueError(f'{string!r} does not end with {suffix!r}')
  return string[:-len(suffix)]


def _GetJavaRoot(path):
  # The authoritative way to determine the Java root for a given source file is
  # to parse the source code and extract the package and class names, but let's
  # keep things simple and use some heuristics to try to guess the Java root
  # from the file path instead.
  while True:
    dirname, basename = os.path.split(path)
    if not basename:
      raise RuntimeError(f'Unable to determine the Java root for {path!r}')
    if basename in ('java', 'src'):
      return path
    if basename in ('javax', 'org', 'com'):
      return dirname
    path = dirname


def _ProcessSourceFile(output_dir, source_file_path, source_dirs):
  source_file_path = os.path.normpath(os.path.join(output_dir,
                                                   source_file_path))
  java_root = _GetJavaRoot(source_file_path)
  # Refuse output_dir/ and output_dir/gen/ as source roots — JDT would scan
  # every sibling and the .classpath would be invalid.
  output_dir_norm = os.path.normpath(output_dir)
  if java_root in (output_dir_norm,
                   os.path.normpath(os.path.join(output_dir_norm, 'gen'))):
    logging.warning('Skipping overly broad Java root `%s` for: %s',
                    java_root, source_file_path)
    return
  logging.debug('Extracted java root `%s` from source file path `%s`',
                java_root, source_file_path)
  source_dirs.add(java_root)


def _ProcessSourcesFile(output_dir, sources_file_path, source_dirs):
  for source_file_path in build_utils.ReadSourcesList(
      os.path.join(output_dir, sources_file_path)):
    _ProcessSourceFile(output_dir, source_file_path, source_dirs)


def _FindCanonicalCompileResources(output_dir, build_config_path):
  """The __compile_resources.srcjar a root target owns, or None.

  Used to pick a single coherent R for the project; see _IsUsefulSrcJar.
  """
  params_json = build_config_path.replace('.build_config.json', '.params.json')
  with open(os.path.join(output_dir, params_json)) as f:
    params = json.load(f)
  for s in params.get('bundled_srcjars', []):
    if s.endswith('__compile_resources.srcjar'):
      return s
  return None


def _IsUsefulSrcJar(srcjar_path, canonical_compile_resources):
  """Drop bundled srcjars that would surface duplicate types in JDT.

  The wrapper-srcjar filters (BuildConfig/NativeLibraries/ProductConfig) hide
  per-apk variants whose classes are already fed via input_srcjars. Resource
  srcjars are pinned to the root target's own to avoid R conflicts across
  libs. Mirrors build/android/chromiumide_api.py:_is_useful_source_jar.
  """
  if srcjar_path.endswith('_placeholder.srcjar'):
    return False
  if srcjar_path.endswith(('__build_config_srcjar.srcjar',
                           '__native_libraries.srcjar',
                           '__product_config_srcjar.srcjar')):
    return False
  if srcjar_path.endswith('__compile_resources.srcjar'):
    return srcjar_path in canonical_compile_resources
  return True


def _ExtractBundledSrcJar(output_dir, srcjar_path, source_dirs):
  """Extract a bundled .srcjar to a cache dir so JDT can index its sources.

  Returns True if a source root was added (False for empty extracts).
  Mirrors build/android/chromiumide_api.py:_extract_source_jar.
  """
  abs_srcjar = os.path.normpath(os.path.join(output_dir, srcjar_path))
  if not os.path.exists(abs_srcjar):
    return False
  extract_dir = os.path.normpath(
      os.path.join(output_dir, 'extracted_srcjars',
                   _WithoutSuffix(srcjar_path, '.srcjar')))
  # Sibling stamp = "extract finished cleanly". Missing/stale stamp triggers
  # re-extract, so a mid-extract crash recovers on the next run.
  stamp = extract_dir + '.stamp'
  if (not os.path.exists(stamp)
      or os.path.getmtime(stamp) < os.path.getmtime(abs_srcjar)):
    if os.path.exists(extract_dir):
      shutil.rmtree(extract_dir)
    os.makedirs(extract_dir)
    with zipfile.ZipFile(abs_srcjar) as zf:
      zf.extractall(extract_dir)
    # Strip placeholder GEN_JNI; every *_jni.srcjar ships one and they collide.
    jni_zero_dir = os.path.join(extract_dir, 'org', 'jni_zero')
    if os.path.exists(jni_zero_dir):
      shutil.rmtree(jni_zero_dir)
    with open(stamp, 'wb'):
      pass
  has_java = any(f.endswith('.java')
                 for _, _, files in os.walk(extract_dir) for f in files)
  if has_java:
    source_dirs.add(extract_dir)
  return has_java


def _AddTargetOutput(output_dir, params_json, build_config, has_sources_file,
                     canonical_compile_resources, source_dirs, libs):
  """Resolve a target's generated/compiled output into source roots and libs.

  Source priority (first match wins; alternatives would dup-type in JDT):
  input_srcjars/ → bundled_srcjars (extracted). The compiled jar is added as
  a classpath lib only for aggregating roots (apk/bundle/robolectric — they
  carry final R/GEN_JNI/BuildConfig classes per
  build/android/docs/java_toolchain.md) or true prebuilts.
  """
  input_srcjars = os.path.join(output_dir,
    _WithoutSuffix(params_json, '.params.json'),
    'generated_java', 'input_srcjars')
  has_source_repr = False
  if os.path.exists(input_srcjars):
    source_dirs.add(input_srcjars)
    has_source_repr = True
  else:
    # Require srcjars to exist on disk so a stale/partial build doesn't
    # suppress the prebuilt fallback below.
    useful_bundled_srcjars = [
      s for s in build_config.get('bundled_srcjars', [])
      if s.startswith('gen/')
      and _IsUsefulSrcJar(s, canonical_compile_resources)
      and os.path.exists(os.path.join(output_dir, s))
    ]
    for srcjar in useful_bundled_srcjars:
      if _ExtractBundledSrcJar(output_dir, srcjar, source_dirs):
        has_source_repr = True

  unprocessed_jar_path = build_config.get('unprocessed_jar_path')
  if unprocessed_jar_path is None:
    return
  is_aggregating = build_config.get('type') in ('android_apk',
                                                'android_app_bundle_module',
                                                'robolectric_binary')
  if is_aggregating or (not has_source_repr and not has_sources_file):
    lib_path = os.path.normpath(os.path.join(output_dir, unprocessed_jar_path))
    logging.debug('Found lib `%s`', lib_path)
    libs.add(lib_path)


def _ProcessBuildConfigFile(output_dir, build_config_path, source_dirs, libs,
                            already_processed_build_config_files,
                            android_sdk_build_tools_version,
                            canonical_compile_resources):
  if build_config_path in already_processed_build_config_files:
    return
  already_processed_build_config_files.add(build_config_path)

  logging.info('Processing build config: %s', build_config_path)

  # deps_configs (needed to walk the dependency tree) lives in
  # .params.json rather than .build_config.json. Read both and merge.
  params_json = build_config_path.replace('.build_config.json', '.params.json')
  with open(os.path.join(output_dir, params_json)) as f:
    build_config = json.load(f)
  build_config_file = os.path.join(output_dir, build_config_path)
  if os.path.exists(build_config_file):
    with open(build_config_file) as f:
      build_config.update(json.load(f))

  target_sources_file = build_config.get('target_sources_file')
  if target_sources_file is not None:
    _ProcessSourcesFile(output_dir, target_sources_file, source_dirs)

  # Add pre-built JARs that are compile-time dependencies (e.g., SDK JARs).
  for jar_path in build_config.get('input_jars_paths', []):
    lib_path = os.path.normpath(os.path.join(output_dir, jar_path))
    if os.path.exists(lib_path):
      logging.debug('Found input jar `%s`', lib_path)
      libs.add(lib_path)

  _AddTargetOutput(output_dir, params_json, build_config,
                   target_sources_file is not None,
                   canonical_compile_resources, source_dirs, libs)

  # Add core-lambda-stubs.jar so that JDT can resolve LambdaMetafactory.
  # d8 normally strips these refs, but JDT doesn't run d8. See:
  #   https://jakewharton.com/androids-java-8-support/#native-lambdas
  lambda_stubs_jar = os.path.join('third_party', 'android_sdk', 'public',
                                  'build-tools',
                                  android_sdk_build_tools_version,
                                  'core-lambda-stubs.jar')
  if os.path.exists(lambda_stubs_jar):
    libs.add(os.path.normpath(lambda_stubs_jar))

  for dep_config in build_config.get('deps_configs', []):
    _ProcessBuildConfigFile(output_dir, dep_config, source_dirs, libs,
                            already_processed_build_config_files,
                            android_sdk_build_tools_version,
                            canonical_compile_resources)
  # Also walk public_deps_configs to pick up transitive dependencies
  # exposed via public_deps (e.g., chrome_public_apk's full dep tree).
  for dep_config in build_config.get('public_deps_configs', []):
    _ProcessBuildConfigFile(output_dir, dep_config, source_dirs, libs,
                            already_processed_build_config_files,
                            android_sdk_build_tools_version,
                            canonical_compile_resources)


def _GenerateClasspathEntry(kind, path, excluding=None):
  classpathentry = xml.etree.ElementTree.Element('classpathentry')
  classpathentry.set('kind', kind)
  classpathentry.set('path', path)
  if excluding:
    classpathentry.set('excluding', excluding)
  return classpathentry


def _GenerateProject(source_dirs, libs, output_dir):
  # Find source dirs that are ancestors of other source dirs. Eclipse JDT
  # does not allow nesting, but supports exclusion patterns to carve out
  # subdirectories that have their own source entries.
  sorted_dirs = sorted(source_dirs)
  # Map parent -> list of nested children (relative to parent)
  nested_children = {}
  for i, d in enumerate(sorted_dirs):
    for other in sorted_dirs[i + 1:]:
      if other.startswith(d + '/'):
        nested_children.setdefault(d, []).append(other[len(d) + 1:] + '/')
      elif not other.startswith(d):
        break

  classpath = xml.etree.ElementTree.Element('classpath')
  for source_dir in sorted(source_dirs):
    children = nested_children.get(source_dir)
    if children:
      classpath.append(
          _GenerateClasspathEntry('src',
                                  source_dir,
                                  excluding='|'.join(children)))
    else:
      classpath.append(_GenerateClasspathEntry('src', source_dir))
  for lib in sorted(libs):
    classpath.append(_GenerateClasspathEntry('lib', lib))
  classpath.append(
    _GenerateClasspathEntry('output', os.path.join(output_dir, 'jdt_output')))

  xml.etree.ElementTree.ElementTree(classpath).write(
    '.classpath', encoding='unicode')
  print('Generated .classpath', file=sys.stderr)

  with open('.project', 'w') as f:
    f.write("""<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
  <name>chromium</name>
  <buildSpec>
    <buildCommand>
      <name>org.eclipse.jdt.core.javabuilder</name>
      <arguments />
    </buildCommand>
  </buildSpec>
  <natures><nature>org.eclipse.jdt.core.javanature</nature></natures>
</projectDescription>
""")
  print('Generated .project', file=sys.stderr)

  # Tell the Eclipse compiler not to use java.lang.invoke.StringConcatFactory
  # in the generated bytecodes as the class is unavailable in Android.
  # Set compiler compliance to match Chromium's javac --release value
  # (read from compile_java.py).
  java_release = _GetJavaReleaseVersion()
  os.makedirs('.settings', exist_ok=True)
  with open('.settings/org.eclipse.jdt.core.prefs', 'w') as f:
    f.write(f"""eclipse.preferences.version=1
org.eclipse.jdt.core.compiler.codegen.useStringConcatFactory=disabled
org.eclipse.jdt.core.compiler.source={java_release}
org.eclipse.jdt.core.compiler.compliance={java_release}
org.eclipse.jdt.core.compiler.codegen.targetPlatform={java_release}
""")
  print('Generated .settings', file=sys.stderr)


def _ParseArguments(argv):
  parser = argparse.ArgumentParser(
      description=
      'Given Chromium Java build config files, generates an Eclipse JDT '
      'project that can be used with the "Language Support for Java™ by '
      'Red Hat" Visual Studio Code extension. See //docs/vscode.md '
      'for details.')
  parser.add_argument(
      '--output-dir',
      required=True,
      help='Relative path to the output directory, e.g. "out/Debug"')
  parser.add_argument(
      '--build-config',
      action='append',
      required=True,
      help='Path to the .build_config.json file to use as input, relative to '
      '`--output-dir`. May be repeated.')
  return parser.parse_args(argv)


def main(argv):
  build_utils.InitLogging('GENERATE_VSCODE_CLASSPATH_DEBUG')

  assert os.path.exists('.gn'), 'This script must be run from the src directory'

  args = _ParseArguments(argv)
  output_dir = args.output_dir

  build_vars = gn_helpers.ReadBuildVars(output_dir)

  # Pin R aggregation to root targets; other __compile_resources.srcjars in
  # the dep tree are partial duplicates that surface R conflicts in JDT.
  canonical_compile_resources = set()
  for build_config_path in args.build_config:
    found = _FindCanonicalCompileResources(output_dir, build_config_path)
    if found is not None:
      canonical_compile_resources.add(found)

  source_dirs = set()
  libs = set()
  already_processed_build_config_files = set()
  for build_config_path in args.build_config:
    _ProcessBuildConfigFile(output_dir, build_config_path, source_dirs, libs,
                            already_processed_build_config_files,
                            build_vars['android_sdk_build_tools_version'],
                            canonical_compile_resources)

  logging.info('Done processing %d build config files',
               len(already_processed_build_config_files))

  _GenerateProject(source_dirs, libs, output_dir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
