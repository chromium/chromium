# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a helper script use by the _swift_modulemap_group GN template.
import os


def get_custom_globals(model):
  metadata = model['metadata']
  modulemap_data = model['modulemap_data']

  return {
      'modules': [
          {
              # Because the group modulemap and child modulemaps may be
              # defined in separate BUILD.gn files, we cannot hard-code any
              # assumptions regarding their relative position in the
              # filesystem.
              'sources': [
                  os.path.relpath(source, metadata['group_dir'])
                  for source in sorted(module['sources'])
              ],
              'name':
              module['name'],
          } for module in sorted(modulemap_data, key=lambda item: item['name'])
      ],
      'target_name':
      metadata['target_name'],
      'module_name':
      metadata['module_name'],
  }


def get_custom_filters(model):
  return {}
