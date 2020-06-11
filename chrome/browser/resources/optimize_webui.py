#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import itertools
import json
import os
import glob
import platform
import re
import shutil
import sys
import tempfile


_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
_CWD = os.getcwd()  # NOTE(dbeam): this is typically out/<gn_name>/.

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules


_RESOURCES_PATH = os.path.join(
    _SRC_PATH, 'ui', 'webui', 'resources', '').replace('\\', '/')


_CR_ELEMENTS_PATH = os.path.join(
    _RESOURCES_PATH, 'cr_elements', '').replace('\\', '/')


_CR_COMPONENTS_PATH = os.path.join(
    _RESOURCES_PATH, 'cr_components', '').replace('\\', '/')


_CSS_RESOURCES_PATH = os.path.join(
    _RESOURCES_PATH, 'css', '').replace('\\', '/')


_HTML_RESOURCES_PATH = os.path.join(
    _RESOURCES_PATH, 'html', '').replace('\\', '/')


_JS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'js', '').replace('\\', '/')


_IMAGES_RESOURCES_PATH = os.path.join(
    _RESOURCES_PATH, 'images', '').replace('\\', '/')


_POLYMER_PATH = os.path.join(
    _SRC_PATH, 'third_party', 'polymer', 'v1_0', 'components-chromium',
    '').replace('\\', '/')


# These files are already combined and minified.
_BASE_EXCLUDES = [
  # Common excludes for both Polymer 2 and 3.
  'chrome://resources/polymer/v1_0/web-animations-js/' +
      'web-animations-next-lite.min.js',
  'chrome://resources/css/roboto.css',
  'chrome://resources/css/text_defaults.css',
  'chrome://resources/css/text_defaults_md.css',
  'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.html',

  # Excludes applying only to Polymer 2.
  'chrome://resources/html/polymer.html',
  'chrome://resources/polymer/v1_0/polymer/polymer.html',
  'chrome://resources/polymer/v1_0/polymer/polymer-micro.html',
  'chrome://resources/polymer/v1_0/polymer/polymer-mini.html',
  'chrome://resources/js/load_time_data.js',

  # Excludes applying only to Polymer 3.
  'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js',
  'chrome://resources/js/load_time_data.m.js',
]

_VULCANIZE_BASE_ARGS = [
  '--inline-css',
  '--inline-scripts',
  '--rewrite-urls-in-templates',
  '--strip-comments',
]

_URL_MAPPINGS = [
    ('chrome://resources/cr_components/', _CR_COMPONENTS_PATH),
    ('chrome://resources/cr_elements/', _CR_ELEMENTS_PATH),
    ('chrome://resources/css/', _CSS_RESOURCES_PATH),
    ('chrome://resources/html/', _HTML_RESOURCES_PATH),
    ('chrome://resources/js/', _JS_RESOURCES_PATH),
    ('chrome://resources/polymer/v1_0/', _POLYMER_PATH),
    ('chrome://resources/images/', _IMAGES_RESOURCES_PATH)
]


_VULCANIZE_REDIRECT_ARGS = list(itertools.chain.from_iterable(map(
    lambda m: ['--redirect', '"%s|%s"' % (m[0], m[1])], _URL_MAPPINGS)))


def _undo_mapping(mappings, url):
  for (redirect_url, file_path) in mappings:
    if url.startswith(redirect_url):
      return url.replace(redirect_url, file_path + os.sep, 1)
  # TODO(dbeam): can we make this stricter?
  return url

def _request_list_path(out_path, host):
  return os.path.join(out_path, host + '_requestlist.txt')

# Get a list of all files that were bundled with polymer-bundler and update the
# depfile accordingly such that Ninja knows when to re-trigger.
def _update_dep_file(in_folder, args, manifest):
  in_path = os.path.join(_CWD, in_folder)

  # Gather the dependencies of all bundled root files.
  request_list = []
  for out_file in manifest:
    request_list += manifest[out_file]

  # Add a slash in front of every dependency that is not a chrome:// URL, so
  # that we can map it to the correct source file path below.
  request_list = map(
      lambda dep: '/' + dep if not dep.startswith('chrome://') else dep,
      request_list)

  # Undo the URL mappings applied by vulcanize to get file paths relative to
  # current working directory.
  url_mappings = _URL_MAPPINGS + [
      ('/', os.path.relpath(in_path, _CWD)),
      ('chrome://%s/' % args.host, os.path.relpath(in_path, _CWD)),
  ]

  deps = [_undo_mapping(url_mappings, u) for u in request_list]
  deps = map(os.path.normpath, deps)

  # If the input was a folder holding an unpacked .pak file, the generated
  # depfile should not list files already in the .pak file.
  if args.input.endswith('.unpak'):
    filter_url = args.input
    deps = [d for d in deps if not d.startswith(filter_url)]

  out_file_name = args.html_out_files[0] if args.html_out_files else \
                  args.js_out_files[0]

  with open(os.path.join(_CWD, args.depfile), 'w') as f:
    deps_file_header = os.path.join(args.out_folder, out_file_name)
    f.write(deps_file_header + ': ' + ' '.join(deps))

# Autogenerate a rollup config file so that we can import the plugin and
# pass it information about the location of the directories and files to exclude
# from the bundle.
def _generate_rollup_config(tmp_out_dir, path_to_plugin, in_path, host,
                            excludes):
  rollup_config_file = os.path.join(tmp_out_dir, 'rollup.config.js')
  excludes_string = '[\'' + '\', \''.join(excludes) + '\']'
  with open(rollup_config_file, 'w') as f:
    f.write('import plugin from \'%s\';\n' % path_to_plugin.replace(
        '\\', '/'))
    f.write('export default({\n')
    f.write('  plugins: [ plugin(\'%s\', \'%s\', \'%s\', \'%s\', %s) ]\n' % (
        _SRC_PATH.replace('\\', '/'),
        os.path.join(_CWD, 'gen').replace('\\', '/'),
        in_path.replace('\\', '/'), host, excludes_string))
    f.write('});')
    f.close()
  return rollup_config_file;

# Create the manifest file from the sourcemap generated by rollup and return the
# list of bundles.
def _generate_manifest_file(tmp_out_dir, in_path, manifest_out_path):
  generated_sourcemaps = glob.glob('%s/*.map' % tmp_out_dir)
  manifest = {}
  output_filenames = []
  for sourcemap_file in generated_sourcemaps:
    with open(sourcemap_file, 'r') as f:
      sourcemap = json.loads(f.read())
      if not 'sources' in sourcemap:
        raise Exception('rollup could not construct source map')
      sources = sourcemap['sources']
      replaced_sources = []
      for source in sources:
        replaced_sources.append(
            source.replace('../' + os.path.basename(in_path) + "/", ""))
      filename = sourcemap_file[:-len('.map')]
      manifest[os.path.basename(filename)] = replaced_sources
      output_filenames.append(filename)

  with open(manifest_out_path, 'w') as f:
    f.write(json.dumps(manifest))
    f.close()

  return output_filenames

def _bundle_v3(tmp_out_dir, in_path, out_path, manifest_out_path, args,
               excludes):
  if not os.path.exists(tmp_out_dir):
    os.makedirs(tmp_out_dir)
  path_to_plugin = os.path.join(
      os.path.abspath(_HERE_PATH), 'tools', 'rollup_plugin.js')
  rollup_config_file = _generate_rollup_config(tmp_out_dir, path_to_plugin,
                                               in_path, args.host, excludes)
  rollup_args = [os.path.join(in_path, f) for f in args.js_module_in_files]

  # Confirm names are as expected. This is necessary to avoid having to replace
  # import statements in the generated output files.
  # TODO(rbpotter): Is it worth adding import statement replacement to support
  # arbitrary names?
  bundled_paths = []
  for index, js_file in enumerate(args.js_module_in_files):
    expected_name = '%s.rollup.js' % js_file[:-len('.js')]
    assert args.js_out_files[index] == expected_name, \
           'Output file corresponding to %s should be named %s.rollup.js' % \
           (js_file, expected_name)
    bundled_paths.append(os.path.join(tmp_out_dir, expected_name))

  # This indicates that rollup is expected to generate a shared chunk file as
  # well as one file per module. Set its name using --chunkFileNames. Note:
  # Currently, this only supports 2 entry points, which generate 2 corresponding
  # outputs and 1 shared output.
  if (len(args.js_out_files) == 3):
    assert len(args.js_module_in_files) == 2, \
           'Expect 2 module entry points for generating 3 outputs'
    shared_file_name = args.js_out_files[2]
    rollup_args += [ '--chunkFileNames', shared_file_name ]
    bundled_paths.append(os.path.join(tmp_out_dir, shared_file_name))

  node.RunNode(
      [node_modules.PathToRollup()] + rollup_args + [
          '--format', 'esm',
          '--dir', tmp_out_dir,
          '--entryFileNames', '[name].rollup.js',
          '--sourcemap', '--sourcemapExcludeSources',
          '--config', rollup_config_file,
          '--silent',
      ])

  # Create the manifest file from the sourcemaps generated by rollup.
  generated_paths = _generate_manifest_file(tmp_out_dir, in_path,
                                            manifest_out_path)
  assert len(generated_paths) == len(bundled_paths), \
         'unexpected number of bundles - %s - generated by rollup' % \
         (len(generated_paths))

  return bundled_paths

def _bundle_v2(tmp_out_dir, in_path, out_path, manifest_out_path, args,
               excludes):
  in_html_args = []
  for f in args.html_in_files:
    in_html_args.append(f)

  exclude_args = []
  for f in excludes:
    exclude_args.append('--exclude')
    exclude_args.append(f);

  node.RunNode(
      [node_modules.PathToBundler()] +
      _VULCANIZE_BASE_ARGS + _VULCANIZE_REDIRECT_ARGS + exclude_args +
      [
       '--manifest-out', manifest_out_path,
       '--root', in_path,
       '--redirect', '"chrome://%s/|%s"' % (args.host, in_path + '/'),
       '--out-dir', os.path.relpath(tmp_out_dir, _CWD).replace('\\', '/'),
       '--shell', args.html_in_files[0],
      ] + in_html_args)

  for index, html_file in enumerate(args.html_in_files):
    with open(os.path.join(
        os.path.relpath(tmp_out_dir, _CWD), html_file), 'r') as f:
      output = f.read()

      # Grit includes are not supported, use HTML imports instead.
      output = output.replace('<include src="', '<include src-disabled="')

      if args.insert_in_head:
        assert '<head>' in output
        # NOTE(dbeam): polymer-bundler eats <base> tags after processing.
        # This undoes that by adding a <base> tag to the (post-processed)
        # generated output.
        output = output.replace('<head>', '<head>' + args.insert_in_head)

    # Open file again with 'w' such that the previous contents are
    # overwritten.
    with open(os.path.join(
        os.path.relpath(tmp_out_dir, _CWD), html_file), 'w') as f:
      f.write(output)
      f.close()

  bundled_paths = []
  for index, html_in_file in enumerate(args.html_in_files):
    bundled_paths.append(
        os.path.join(tmp_out_dir, args.html_out_files[index]))
    js_out_file = args.js_out_files[index]

    # Run crisper to separate the JS from the HTML file.
    node.RunNode([node_modules.PathToCrisper(),
                 '--source', os.path.join(tmp_out_dir, html_in_file),
                 '--script-in-head', 'false',
                 '--html', bundled_paths[index],
                 '--js', os.path.join(tmp_out_dir, js_out_file)])
  return bundled_paths

def _optimize(in_folder, args):
  in_path = os.path.normpath(os.path.join(_CWD, in_folder)).replace('\\', '/')
  out_path = os.path.join(_CWD, args.out_folder).replace('\\', '/')
  manifest_out_path = _request_list_path(out_path, args.host)
  tmp_out_dir = tempfile.mkdtemp(dir=out_path).replace('\\', '/')

  excludes = _BASE_EXCLUDES + [
    # This file is dynamically created by C++. Need to specify an exclusion
    # URL for both the relative URL and chrome:// URL syntax.
    'strings.js',
    'strings.m.js',
    'chrome://%s/strings.js' % args.host,
    'chrome://%s/strings.m.js' % args.host,
  ]
  excludes.extend(args.exclude or [])

  try:
    if args.js_module_in_files:
      pcb_out_paths = [os.path.join(tmp_out_dir, f) for f in args.js_out_files]
      bundled_paths = _bundle_v3(tmp_out_dir, in_path, out_path,
                                 manifest_out_path, args, excludes)
    else:
      pcb_out_paths = [os.path.join(out_path, f) for f in args.html_out_files]
      bundled_paths = _bundle_v2(tmp_out_dir, in_path, out_path,
                                 manifest_out_path, args, excludes)

    # Run polymer-css-build.
    node.RunNode([node_modules.PathToPolymerCssBuild()] +
                 ['--polymer-version', '2'] +
                 ['--no-inline-includes', '-f'] +
                 bundled_paths + ['-o'] + pcb_out_paths)

    # Pass the JS files through Uglify and write the output to its final
    # destination.
    for index, js_out_file in enumerate(args.js_out_files):
      node.RunNode([node_modules.PathToUglify(),
                    os.path.join(tmp_out_dir, js_out_file),
                    '--comments', '"/Copyright|license|LICENSE|\<\/?if/"',
                    '--output', os.path.join(out_path, js_out_file)])
  finally:
    shutil.rmtree(tmp_out_dir)
  return manifest_out_path

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--depfile', required=True)
  parser.add_argument('--exclude', nargs='*')
  parser.add_argument('--host', required=True)
  parser.add_argument('--html_in_files', nargs='*')
  parser.add_argument('--html_out_files', nargs='*')
  parser.add_argument('--input', required=True)
  parser.add_argument('--insert_in_head')
  parser.add_argument('--js_out_files', nargs='*', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_module_in_files', nargs='*')
  args = parser.parse_args(argv)

  # Either JS module input files (for Polymer 3) or HTML input and output files
  # (for Polymer 2) should be provided.
  is_polymer2 = bool(args.html_out_files) and bool(args.html_in_files)
  is_polymer3 = bool(args.js_module_in_files)
  assert(is_polymer2 != is_polymer3)

  # NOTE(dbeam): on Windows, GN can send dirs/like/this. When joined, you might
  # get dirs/like/this\file.txt. This looks odd to windows. Normalize to right
  # the slashes.
  args.depfile = os.path.normpath(args.depfile)
  args.input = os.path.normpath(args.input)
  args.out_folder = os.path.normpath(args.out_folder)

  manifest_out_path = _optimize(args.input, args)

  # Prior call to _optimize() generated an output manifest file, containing
  # information about all files that were bundled. Grab it from there.
  manifest = json.loads(open(manifest_out_path, 'r').read())

  # polymer-bundler reports any missing files in the output manifest, instead of
  # directly failing. Ensure that no such files were encountered.
  if '_missing' in manifest:
    raise Exception(
        'polymer-bundler could not find files for the following URLs:\n' +
        '\n'.join(manifest['_missing']))

  _update_dep_file(args.input, args, manifest)


if __name__ == '__main__':
  main(sys.argv[1:])
