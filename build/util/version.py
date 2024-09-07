#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
version.py -- Chromium version string substitution utility.
"""


import argparse
import os
import stat
import sys

import android_chrome_version


def FetchValuesFromFile(values_dict, file_name):
  """
  Fetches KEYWORD=VALUE settings from the specified file.

  Everything to the left of the first '=' is the keyword,
  everything to the right is the value.  No stripping of
  white space, so beware.

  The file must exist, otherwise you get the Python exception from open().
  """
  with open(file_name, 'r') as f:
    for line in f.readlines():
      key, val = line.rstrip('\r\n').split('=', 1)
      values_dict[key] = val


def FetchValues(file_list, is_official_build=None):
  """
  Returns a dictionary of values to be used for substitution.

  Populates the dictionary with KEYWORD=VALUE settings from the files in
  'file_list'.

  Explicitly adds the following value from internal calculations:

    OFFICIAL_BUILD
  """
  CHROME_BUILD_TYPE = os.environ.get('CHROME_BUILD_TYPE')
  if CHROME_BUILD_TYPE == '_official' or is_official_build:
    official_build = '1'
  else:
    official_build = '0'

  values = dict(
    OFFICIAL_BUILD = official_build,
  )

  for file_name in file_list:
    FetchValuesFromFile(values, file_name)

  script_dirname = os.path.dirname(os.path.realpath(__file__))
  if official_build == '1':
    lastchange_filename = os.path.join(script_dirname, "LASTCHANGE")
  else:
    lastchange_filename = os.path.join(script_dirname, "LASTCHANGE.dummy")
  lastchange_values = {}
  FetchValuesFromFile(lastchange_values, lastchange_filename)

  for placeholder_key, placeholder_value in values.items():
    values[placeholder_key] = SubstTemplate(placeholder_value,
                                            lastchange_values)

  return values


def SubstTemplate(contents, values):
  """
  Returns the template with substituted values from the specified dictionary.

  Keywords to be substituted are surrounded by '@':  @KEYWORD@.

  No attempt is made to avoid recursive substitution.  The order
  of evaluation is random based on the order of the keywords returned
  by the Python dictionary.  So do NOT substitute a value that
  contains any @KEYWORD@ strings expecting them to be recursively
  substituted, okay?
  """
  for key, val in values.items():
    try:
      contents = contents.replace('@' + key + '@', val)
    except TypeError:
      print(repr(key), repr(val))
  return contents


def SubstFile(file_name, values):
  """
  Returns the contents of the specified file_name with substituted values.

  Substituted values come from the specified dictionary.

  This is like SubstTemplate, except it operates on a file.
  """
  with open(file_name, 'r') as f:
    template = f.read()
  return SubstTemplate(template, values)


def WriteIfChanged(file_name, contents, mode):
  """
  Writes the specified contents to the specified file_name.

  Does nothing if the contents aren't different than the current contents.
  """
  try:
    with open(file_name, 'r') as f:
      old_contents = f.read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents and mode == stat.S_IMODE(
        os.lstat(file_name).st_mode):
      return
    os.unlink(file_name)
  with open(file_name, 'w') as f:
    f.write(contents)
  os.chmod(file_name, mode)


def BuildParser():
  """Build argparse parser, with added arguments."""
  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--file', action='append', default=[],
                      help='Read variables from FILE.')
  parser.add_argument('-i', '--input', default=None,
                      help='Read strings to substitute from FILE.')
  parser.add_argument('-o', '--output', default=None,
                      help='Write substituted strings to FILE.')
  parser.add_argument('-t', '--template', default=None,
                      help='Use TEMPLATE as the strings to substitute.')
  parser.add_argument('-x',
                      '--executable',
                      default=False,
                      action='store_true',
                      help='Set the executable bit on the output (on POSIX).')
  parser.add_argument(
      '-e',
      '--eval',
      action='append',
      default=[],
      help='Evaluate VAL after reading variables. Can be used '
      'to synthesize variables. e.g. -e \'PATCH_HI=int('
      'PATCH)//256.')
  parser.add_argument(
      '-a',
      '--arch',
      default=None,
      choices=android_chrome_version.ARCH_CHOICES,
      help='Set which cpu architecture the build is for.')
  parser.add_argument('--os', default=None, help='Set the target os.')
  parser.add_argument('--official', action='store_true',
                      help='Whether the current build should be an official '
                           'build, used in addition to the environment '
                           'variable.')
  parser.add_argument('--next',
                      action='store_true',
                      help='Whether the current build should be a "next" '
                      'build, which targets pre-release versions of Android.')
  parser.add_argument('args', nargs=argparse.REMAINDER,
                      help='For compatibility: INPUT and OUTPUT can be '
                           'passed as positional arguments.')
  return parser


def BuildEvals(options, parser):
  """Construct a dict of passed '-e' arguments for evaluating."""
  evals = {}
  for expression in options.eval:
    try:
      evals.update(dict([expression.split('=', 1)]))
    except ValueError:
      parser.error('-e requires VAR=VAL')
  return evals


def ModifyOptionsCompat(options, parser):
  """Support compatibility with old versions.

  Specifically, for old versions that considered the first two
  positional arguments shorthands for --input and --output.
  """
  while len(options.args) and (options.input is None or options.output is None):
    if options.input is None:
      options.input = options.args.pop(0)
    elif options.output is None:
      options.output = options.args.pop(0)
  if options.args:
    parser.error('Unexpected arguments: %r' % options.args)


def GenerateValues(options, evals):
  """Construct a dict of raw values used to generate output.

  e.g. this could return a dict like
  {
    'BUILD': 74,
  }

  which would be used to resolve a template like
  'build = "@BUILD@"' into 'build = "74"'

  """
  values = FetchValues(options.file, options.official)

  for key, val in evals.items():
    values[key] = str(eval(val, globals(), values))

  if options.os == 'android':
    android_chrome_version_codes = android_chrome_version.GenerateVersionCodes(
        int(values['BUILD']), int(values['PATCH']), options.arch, options.next)
    values.update(android_chrome_version_codes)

  return values


def GenerateOutputContents(options, values):
  """Construct output string (e.g. from template).

  Arguments:
  options -- argparse parsed arguments
  values -- dict with raw values used to resolve the keywords in a template
    string
  """

  if options.template is not None:
    return SubstTemplate(options.template, values)
  elif options.input:
    return SubstFile(options.input, values)
  else:
    # Generate a default set of version information.
    return """MAJOR=%(MAJOR)s
MINOR=%(MINOR)s
BUILD=%(BUILD)s
PATCH=%(PATCH)s
LASTCHANGE=%(LASTCHANGE)s
OFFICIAL_BUILD=%(OFFICIAL_BUILD)s
""" % values


def GenerateOutputMode(options):
  """Construct output mode (e.g. from template).

  Arguments:
  options -- argparse parsed arguments
  """
  if options.executable:
    return 0o755
  else:
    return 0o644


def BuildOutput(args):
  """Gets all input and output values needed for writing output."""
  # Build argparse parser with arguments
  parser = BuildParser()
  options = parser.parse_args(args)

  # Get dict of passed '-e' arguments for evaluating
  evals = BuildEvals(options, parser)
  # For compatibility with interface that considered first two positional
  # arguments shorthands for --input and --output.
  ModifyOptionsCompat(options, parser)

  # Get the raw values that will be used the generate the output
  values = GenerateValues(options, evals)
  # Get the output string and mode
  contents = GenerateOutputContents(options, values)
  mode = GenerateOutputMode(options)

  return {'options': options, 'contents': contents, 'mode': mode}


def main(args):
  output = BuildOutput(args)

  if output['options'].output is not None:
    WriteIfChanged(output['options'].output, output['contents'], output['mode'])
  else:
    print(output['contents'])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
