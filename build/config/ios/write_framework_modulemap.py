# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

MODULE_MAP_TEMPLATE = '''\
framework module %(framework_name)s {
  umbrella header "%(framework_name)s.h"

  export *
  module * { export * }
}
'''


def Main(framework_name, modules_dir):
  # Find the name of the binary based on the part before the ".framework".
  if not os.path.isdir(modules_dir):
    os.makedirs(modules_dir)

  with open(os.path.join(modules_dir, 'module.modulemap'), 'w') as module_file:
    module_file.write(MODULE_MAP_TEMPLATE % {'framework_name': framework_name})


if __name__ == '__main__':
  Main(*sys.argv[1:])
