#!/usr/bin/env python2.7
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for doing Java refactors over native methods.

Converts
(a) non-static natives to static natives using @JCaller
e.g.
class A {
  native void nativeFoo(int a);

  void bar() {
    nativeFoo(5);
  }
}
->
import .....JCaller
class A {
  static native void nativeFoo(@JCaller caller, int a);

  void bar() {
    nativeFoo(A.this, 5);
  }
}

(b) static natives to new mockable static natives.
e.g.
class A {
  static native void nativeFoo(@JCaller caller, int a);

  void bar() {
    nativeFoo(5);
  }
}
->
import .....JCaller
class A {
  void bar() {
    AJni.get().foo(A.this, 5);
  }

  @NativeMethods
  interface Natives {
    static native void foo(@JCaller caller, int a);
  }
}

Created for large refactors to @NativeMethods.
Note: This tool does most of the heavy lifting in the conversion but
there are some things that are difficult to implement with regex and
infrequent enough that they can be done by hand.

These exceptions are:
1) public native methods calls used in other classes are not refactored.
2) native methods inherited from a super class are not refactored
3) Non-static methods are always assumed to be called by the class instance
instead of by another class using that object.
"""

from __future__ import print_function

import argparse
import sys
import string
import re
import os
import pickle

import jni_generator

_JNI_INTERFACE_TEMPLATES = string.Template("""
    @NativeMethods
    interface ${INTERFACE_NAME} {${METHODS}
    }
""")

_JNI_METHOD_DECL = string.Template("""
        ${RETURN_TYPE} ${NAME}($PARAMS);""")

_COMMENT_REGEX_STRING = r'(?:(?:(?:\/\*[^\/]*\*\/)+|(?:\/\/[^\n]*?\n))+\s*)*'

_NATIVES_REGEX = re.compile(
    r'(?P<comments>' + _COMMENT_REGEX_STRING + ')'
    r'(?P<annotations>(@NativeClassQualifiedName'
    r'\(\"(?P<native_class_name>.*?)\"\)\s+)?'
    r'(@NativeCall(\(\"(?P<java_class_name>.*?)\"\))\s+)?)'
    r'(?P<qualifiers>\w+\s\w+|\w+|\s+)\s+static\s*native '
    r'(?P<return_type>\S*) '
    r'(?P<name>native\w+)\((?P<params>.*?)\);\n', re.DOTALL)

_NON_STATIC_NATIVES_REGEX = re.compile(
    r'(?P<comments>' + _COMMENT_REGEX_STRING + ')'
    r'(?P<annotations>(@NativeClassQualifiedName'
    r'\(\"(?P<native_class_name>.*?)\"\)\s+)?'
    r'(@NativeCall(\(\"(?P<java_class_name>.*?)\"\))\s+)?)'
    r'(?P<qualifiers>\w+\s\w+|\w+|\s+)\s*native '
    r'(?P<return_type>\S*) '
    r'(?P<name>native\w+)\((?P<params>.*?)\);\n', re.DOTALL)
_NATIVE_PTR_REGEX = re.compile(r'\s*long native.*')

JNI_IMPORT_STRING = 'import org.chromium.base.annotations.NativeMethods;'
IMPORT_REGEX = re.compile(r'^import .*?;', re.MULTILINE)

PICKLE_LOCATION = './jni_ref_pickle'


def build_method_declaration(return_type, name, params, annotations, comments):
  out = _JNI_METHOD_DECL.substitute({
      'RETURN_TYPE': return_type,
      'NAME': name,
      'PARAMS': params
  })
  if annotations:
    out = '\n' + annotations + out
  if comments:
    out = '\n' + comments + out
  if annotations or comments:
    out += '\n'
  return out


def add_chromium_import_to_java_file(contents, import_string):
  # Just in cases there are no imports default to after the package statement.
  import_insert = contents.find(';') + 1

  # Insert in alphabetical order into org.chromium. This assumes
  # that all files will contain some org.chromium import.
  for match in IMPORT_REGEX.finditer(contents):
    import_name = match.group()

    if not 'import org.chromium' in import_name:
      continue
    if import_name > import_insert:
      import_insert = match.start()
      break
    else:
      import_insert = match.end() + 1

  return "%s%s\n%s" % (contents[:import_insert], import_string,
                       contents[import_insert:])


def convert_nonstatic_to_static(java_file_name,
                                skip_caller=False,
                                dry=False,
                                verbose=True):
  if java_file_name is None:
    return
  if not os.path.isfile(java_file_name):
    if verbose:
      print('%s does not exist', java_file_name)
    return

  with open(java_file_name, 'r') as f:
    contents = f.read()

  no_comment_content = jni_generator.RemoveComments(contents)
  parsed_natives = jni_generator.ExtractNatives(no_comment_content, 'long')
  non_static_natives = [n for n in parsed_natives if not n.static]
  if not non_static_natives:
    if verbose:
      print('no natives found')
    return

  class_name = jni_generator.ExtractFullyQualifiedJavaClassName(
      java_file_name, no_comment_content).split('/')[-1]

  # For fixing call sites.
  replace_patterns = []
  should_append_comma = []
  should_prepend_comma = []

  new_contents = contents

  # 1. Change non-static -> static.
  insertion_offset = 0

  matches = []
  for match in _NON_STATIC_NATIVES_REGEX.finditer(contents):
    if not 'static' in match.group('qualifiers'):
      matches.append(match)
      # Insert static as a keyword.
      qual_end = match.end('qualifiers') + insertion_offset
      insert_str = ' static '
      new_contents = new_contents[:qual_end] + insert_str + new_contents[
          qual_end:]
      insertion_offset += len(insert_str)

      if skip_caller:
        continue

      # Insert an object param.
      insert_str = '%s caller' % class_name
      # No params.
      if not match.group('params'):
        start = insertion_offset + match.start('params')
        append_comma = False
        prepend_comma = False

      # One or more params.
      else:
        # Has mNativePtr.
        if _NATIVE_PTR_REGEX.match(match.group('params')):
          # Only 1 param, append to end of params.
          if match.group('params').count(',') == 0:
            start = insertion_offset + match.end('params')
            append_comma = False
            prepend_comma = True
          # Multiple params, insert after first param.
          else:
            comma_pos = match.group('params').find(',')
            start = insertion_offset + match.start('params') + comma_pos + 1
            append_comma = True
            prepend_comma = False
        else:
          # No mNativePtr, insert as first param.
          start = insertion_offset + match.start('params')
          append_comma = True
          prepend_comma = False

      if prepend_comma:
        insert_str = ', ' + insert_str
      if append_comma:
        insert_str = insert_str + ', '
      new_contents = new_contents[:start] + insert_str + new_contents[start:]

      # Match lines that don't have a native keyword.
      native_match = r'\((\s*?(([ms]Native\w+)|([ms]\w+Android(Ptr)?)),?)?)'
      replace_patterns.append(r'(^\s*' + match.group('name') + native_match)
      replace_patterns.append(r'(return ' + match.group('name') + native_match)
      replace_patterns.append(r'([\:\)\(\+\*\?\&\|,\.\-\=\!\/][ \t]*' +
                              match.group('name') + native_match)

      should_append_comma.extend([append_comma] * 3)
      should_prepend_comma.extend([prepend_comma] * 3)
      insertion_offset += len(insert_str)

  assert len(matches) == len(non_static_natives), ('Regex missed a native '
                                                   'method that was found by '
                                                   'the jni_generator.')

  # 2. Add a this param to all calls.
  for i, r in enumerate(replace_patterns):
    prepend_comma = ', ' if should_prepend_comma[i] else ''
    append_comma = ', ' if should_append_comma[i] else ''
    repl_str = '\g<1>' + prepend_comma + ' %s.this' + append_comma
    new_contents = re.sub(
        r, repl_str % class_name, new_contents, flags=re.MULTILINE)

  if dry:
    print(new_contents)
  else:
    with open(java_file_name, 'w') as f:
      f.write(new_contents)


def filter_files_with_natives(files, verbose=True):
  filtered = []
  i = 1
  for java_file_name in files:
    if not os.path.isfile(java_file_name):
      print('does not exist')
      return
    if verbose:
      print('Processing %s/%s - %s ' % (i, len(files), java_file_name))
    with open(java_file_name, 'r') as f:
      contents = f.read()
    no_comment_content = jni_generator.RemoveComments(contents)
    natives = jni_generator.ExtractNatives(no_comment_content, 'long')

    if len(natives) > 1:
      filtered.append(java_file_name)
    i += 1
  return filtered


def convert_file_to_proxy_natives(java_file_name, dry=False, verbose=True):
  if not os.path.isfile(java_file_name):
    if verbose:
      print('%s does not exist', java_file_name)
    return

  with open(java_file_name, 'r') as f:
    contents = f.read()

  no_comment_content = jni_generator.RemoveComments(contents)
  natives = jni_generator.ExtractNatives(no_comment_content, 'long')

  static_natives = [n for n in natives if n.static]
  if not static_natives:
    if verbose:
      print('%s has no static natives.', java_file_name)
    return

  contents = add_chromium_import_to_java_file(contents, JNI_IMPORT_STRING)

  # Extract comments and annotations above native methods.
  native_map = {}
  for itr in re.finditer(_NATIVES_REGEX, contents):
    n_dict = {}
    n_dict['annotations'] = itr.group('annotations').strip()
    n_dict['comments'] = itr.group('comments').strip()
    n_dict['params'] = itr.group('params').strip()
    native_map[itr.group('name')] = n_dict

  # Using static natives here ensures all the methods that are picked up by
  # the JNI generator are also caught by our own regex.
  methods = []
  for n in static_natives:
    new_name = n.name[0].lower() + n.name[1:]
    n_dict = native_map['native' + n.name]
    params = n_dict['params']
    comments = n_dict['comments']
    annotations = n_dict['annotations']
    methods.append(
        build_method_declaration(n.return_type, new_name, params, annotations,
                                 comments))

  fully_qualified_class = jni_generator.ExtractFullyQualifiedJavaClassName(
      java_file_name, contents)
  class_name = fully_qualified_class.split('/')[-1]
  jni_class_name = class_name + 'Jni'

  # Remove all old declarations.
  for n in static_natives:
    pattern = _NATIVES_REGEX
    contents = re.sub(pattern, '', contents)

  # Replace occurences with new signature.
  for n in static_natives:
    # Okay not to match first (.
    # Since even if natives share a prefix, the replacement is the same.
    # E.g. if nativeFoo() and nativeFooBar() are both statics
    # and in nativeFooBar() we replace nativeFoo -> AJni.get().foo
    # the result is the same as replacing nativeFooBar() -> AJni.get().fooBar
    pattern = r'native%s' % n.name
    lower_name = n.name[0].lower() + n.name[1:]
    contents = re.sub(pattern, '%s.get().%s' % (jni_class_name, lower_name),
                      contents)

  # Build and insert the @NativeMethods interface.
  interface = _JNI_INTERFACE_TEMPLATES.substitute({
      'INTERFACE_NAME': 'Natives',
      'METHODS': ''.join(methods)
  })

  # Insert the interface at the bottom of the top level class.
  # Most of the time this will be before the last }.
  insertion_point = contents.rfind('}')
  contents = contents[:insertion_point] + '\n' + interface + contents[
      insertion_point:]

  if not dry:
    with open(java_file_name, 'w') as f:
      f.write(contents)
  else:
    print(contents)
  return contents


def main(argv):
  arg_parser = argparse.ArgumentParser()

  mutually_ex_group = arg_parser.add_mutually_exclusive_group()

  mutually_ex_group.add_argument(
      '-R',
      '--recursive',
      action='store_true',
      help='Run recursively over all java files '
      'descendants of the current directory.',
      default=False)
  mutually_ex_group.add_argument(
      '--read_cache',
      help='Reads paths to refactor from pickled file %s.' % PICKLE_LOCATION,
      action='store_true',
      default=False)
  mutually_ex_group.add_argument(
      '--source', help='Path to refactor single source file.', default=None)

  arg_parser.add_argument(
      '--cache',
      action='store_true',
      help='Finds all java files with native functions recursively from '
      'the current directory, then pickles and saves them to %s and then'
      'exits.' % PICKLE_LOCATION,
      default=False)
  arg_parser.add_argument(
      '--dry_run',
      default=False,
      action='store_true',
      help='Print refactor output to console instead '
      'of replacing the contents of files.')
  arg_parser.add_argument(
      '--nonstatic',
      default=False,
      action='store_true',
      help='If true converts native nonstatic methods to static methods'
      ' instead of converting static methods to new jni.')
  arg_parser.add_argument(
      '--verbose', default=False, action='store_true', help='')
  arg_parser.add_argument(
      '--ignored-paths',
      action='append',
      help='Paths to ignore during conversion.')
  arg_parser.add_argument(
      '--skip-caller-arg',
      default=False,
      action='store_true',
      help='Do not add the "this" param when converting non-static methods.')

  args = arg_parser.parse_args()

  java_file_paths = []

  if args.source:
    java_file_paths = [args.source]
  elif args.read_cache:
    print('Reading paths from ' + PICKLE_LOCATION)
    with open(PICKLE_LOCATION, 'r') as file:
      java_file_paths = pickle.load(file)
      print('Found %s java paths.' % len(java_file_paths))
  elif args.recursive:
    for root, _, files in os.walk(os.path.abspath('.')):
      java_file_paths.extend(
          ['%s/%s' % (root, f) for f in files if f.endswith('.java')])

  else:
    # Get all java files in current dir.
    java_file_paths = filter(lambda x: x.endswith('.java'),
                             map(os.path.abspath, os.listdir('.')))

  if args.ignored_paths:
    java_file_paths = [
        path for path in java_file_paths
        if all(p not in path for p in args.ignored_paths)
    ]

  if args.cache:
    with open(PICKLE_LOCATION, 'w') as file:
      pickle.dump(filter_files_with_natives(java_file_paths), file)
      print('Java files with proxy natives written to ' + PICKLE_LOCATION)

  i = 1
  for f in java_file_paths:
    print(f)
    if args.nonstatic:
      convert_nonstatic_to_static(
          f,
          skip_caller=args.skip_caller_arg,
          dry=args.dry_run,
          verbose=args.verbose)
    else:
      convert_file_to_proxy_natives(f, dry=args.dry_run, verbose=args.verbose)
    print('Done converting %s/%s' % (i, len(java_file_paths)))
    i += 1

  print('Done please run git cl format.')


if __name__ == '__main__':
  sys.exit(main(sys.argv))
