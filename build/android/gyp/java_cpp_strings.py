#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

from util import build_utils
from util import java_cpp_utils


class StringParserDelegate(java_cpp_utils.CppConstantParser.Delegate):
  STRING_RE = re.compile(r'\s*const char k(.*)\[\]\s*=')
  VALUE_RE = re.compile(r'\s*("(?:\"|[^"])*")\s*;')

  def ExtractConstantName(self, line):
    match = StringParserDelegate.STRING_RE.match(line)
    return match.group(1) if match else None

  def ExtractValue(self, line):
    match = StringParserDelegate.VALUE_RE.search(line)
    return match.group(1) if match else None

  def CreateJavaConstant(self, name, value, comments):
    return java_cpp_utils.JavaString(name, value, comments)


def _GenerateOutput(template, source_paths, template_path, strings):
  description_template = """
    // This following string constants were inserted by
    //     {SCRIPT_NAME}
    // From
    //     {SOURCE_PATHS}
    // Into
    //     {TEMPLATE_PATH}

"""
  values = {
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATHS': ',\n    //     '.join(source_paths),
      'TEMPLATE_PATH': template_path,
  }
  description = description_template.format(**values)
  native_strings = '\n\n'.join(x.Format() for x in strings)

  values = {
      'NATIVE_STRINGS': description + native_strings,
  }
  return template.format(**values)


def _ParseStringFile(path):
  with open(path) as f:
    string_file_parser = java_cpp_utils.CppConstantParser(
        StringParserDelegate(), f.readlines())
  return string_file_parser.Parse()


def _Generate(source_paths, template_path):
  with open(template_path) as f:
    lines = f.readlines()

  template = ''.join(lines)
  package, class_name = java_cpp_utils.ParseTemplateFile(lines)
  output_path = java_cpp_utils.GetJavaFilePath(package, class_name)
  strings = []
  for source_path in source_paths:
    strings.extend(_ParseStringFile(source_path))

  output = _GenerateOutput(template, source_paths, template_path, strings)
  return output, output_path


def _Main(argv):
  parser = argparse.ArgumentParser()

  parser.add_argument('--srcjar',
                      required=True,
                      help='The path at which to generate the .srcjar file')

  parser.add_argument('--template',
                      required=True,
                      help='The template file with which to generate the Java '
                      'class. Must have "{NATIVE_STRINGS}" somewhere in '
                      'the template.')

  parser.add_argument(
      'inputs', nargs='+', help='Input file(s)', metavar='INPUTFILE')
  args = parser.parse_args(argv)

  with build_utils.AtomicOutput(args.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      data, path = _Generate(args.inputs, args.template)
      build_utils.AddToZipHermetic(srcjar, path, data=data)


if __name__ == '__main__':
  _Main(sys.argv[1:])
