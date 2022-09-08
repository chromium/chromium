#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Allots libraries to modules to be packaged into.

All libraries that are depended on by a single module will be allotted to this
module. All other libraries will be allotted to the closest ancestor.

Example:
  Given the module dependency structure

        c
       / \
      b   d
     /     \
    a       e

  and libraries assignment

    a: ['lib1.so']
    e: ['lib2.so', 'lib1.so']

  will make the allotment decision

    c: ['lib1.so']
    e: ['lib2.so']

  The above example is invoked via:

    ./allot_native_libraries \
      --libraries 'a,["1.so"]' \
      --libraries 'e,["2.so", "1.so"]' \
      --dep c:b \
      --dep b:a \
      --dep c:d \
      --dep d:e \
      --output <output JSON>
"""

import argparse
import collections
import json
import sys

from util import build_utils


def _ModuleLibrariesPair(arg):
  pos = arg.find(',')
  assert pos > 0
  return (arg[:pos], arg[pos + 1:])


def _DepPair(arg):
  parent, child = arg.split(':')
  return (parent, child)


def _PathFromRoot(module_tree, module):
  """Computes path from root to a module.

  Parameters:
    module_tree: Dictionary mapping each module to its parent.
    module: Module to which to compute the path.

  Returns:
    Path from root the the module.
  """
  path = [module]
  while module_tree.get(module):
    module = module_tree[module]
    path = [module] + path
  return path


def _ClosestCommonAncestor(module_tree, modules):
  """Computes the common ancestor of a set of modules.

  Parameters:
    module_tree: Dictionary mapping each module to its parent.
    modules: Set of modules for which to find the closest common ancestor.

  Returns:
    The closest common ancestor.
  """
  paths = [_PathFromRoot(module_tree, m) for m in modules]
  assert len(paths) > 0
  ancestor = None
  for level in zip(*paths):
    if len(set(level)) != 1:
      return ancestor
    ancestor = level[0]
  return ancestor


def _AllotLibraries(module_tree, libraries_map):
  """Allot all libraries to a module.

  Parameters:
    module_tree: Dictionary mapping each module to its parent. Modules can map
      to None, which is considered the root of the tree.
    libraries_map: Dictionary mapping each library to a set of modules, which
      depend on the library.

  Returns:
    A dictionary mapping mapping each module name to a set of libraries allotted
    to the module such that libraries with multiple dependees are allotted to
    the closest ancestor.

  Raises:
    Exception if some libraries can only be allotted to the None root.
  """
  allotment_map = collections.defaultdict(set)
  for library, modules in libraries_map.items():
    ancestor = _ClosestCommonAncestor(module_tree, modules)
    if not ancestor:
      raise Exception('Cannot allot libraries for given dependency tree')
    allotment_map[ancestor].add(library)
  return allotment_map


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--libraries',
      action='append',
      type=_ModuleLibrariesPair,
      required=True,
      help='A pair of module name and GN list of libraries a module depends '
      'on. Can be specified multiple times.')
  parser.add_argument(
      '--output',
      required=True,
      help='A JSON file with a key for each module mapping to a list of '
      'libraries, which should be packaged into this module.')
  parser.add_argument(
      '--dep',
      action='append',
      type=_DepPair,
      dest='deps',
      default=[],
      help='A pair of parent module name and child module name '
      '(format: "<parent>:<child>"). Can be specified multiple times.')
  options = parser.parse_args(build_utils.ExpandFileArgs(args))
  options.libraries = [(m, build_utils.ParseGnList(l))
                       for m, l in options.libraries]

  # Parse input creating libraries and dependency tree.
  libraries_map = collections.defaultdict(set)  # Maps each library to its
  #                                               dependee modules.
  module_tree = {}  # Maps each module name to its parent.
  for module, libraries in options.libraries:
    module_tree[module] = None
    for library in libraries:
      libraries_map[library].add(module)
  for parent, child in options.deps:
    if module_tree.get(child):
      raise Exception('%s cannot have multiple parents' % child)
    module_tree[child] = parent
    module_tree[parent] = module_tree.get(parent)

  # Allot all libraries to a module such that libraries with multiple dependees
  # are allotted to the closest ancestor.
  allotment_map = _AllotLibraries(module_tree, libraries_map)

  # The build system expects there to be a set of libraries even for the modules
  # that don't have any libraries allotted.
  for module in module_tree:
    # Creates missing sets because of defaultdict.
    allotment_map[module] = allotment_map[module]

  with open(options.output, 'w') as f:
    # Write native libraries config and ensure the output is deterministic.
    json.dump({m: sorted(l)
               for m, l in allotment_map.items()},
              f,
              sort_keys=True,
              indent=2)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
