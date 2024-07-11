# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions useful when writing scripts that integrate with GN.

The main functions are ToGNString() and FromGNString(), to convert between
serialized GN veriables and Python variables.

To use in an arbitrary Python file in the build:

  import os
  import sys

  sys.path.append(os.path.join(os.path.dirname(__file__),
                               os.pardir, os.pardir, 'build'))
  import gn_helpers

Where the sequence of parameters to join is the relative path from your source
file to the build directory.
"""

import json
import os
import re
import shutil
import sys


_CHROMIUM_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

BUILD_VARS_FILENAME = 'build_vars.json'
IMPORT_RE = re.compile(r'^import\("//(\S+)"\)')


class GNError(Exception):
  pass


# Computes ASCII code of an element of encoded Python 2 str / Python 3 bytes.
_Ord = ord if sys.version_info.major < 3 else lambda c: c


def _TranslateToGnChars(s):
  for decoded_ch in s.encode('utf-8'):  # str in Python 2, bytes in Python 3.
    code = _Ord(decoded_ch)  # int
    if code in (34, 36, 92):  # For '"', '$', or '\\'.
      yield '\\' + chr(code)
    elif 32 <= code < 127:
      yield chr(code)
    else:
      yield '$0x%02X' % code


def ToGNString(value, pretty=False):
  """Returns a stringified GN equivalent of a Python value.

  Args:
    value: The Python value to convert.
    pretty: Whether to pretty print. If true, then non-empty lists are rendered
        recursively with one item per line, with indents. Otherwise lists are
        rendered without new line.
  Returns:
    The stringified GN equivalent to |value|.

  Raises:
    GNError: |value| cannot be printed to GN.
  """

  if sys.version_info.major < 3:
    basestring_compat = basestring
  else:
    basestring_compat = str

  # Emits all output tokens without intervening whitespaces.
  def GenerateTokens(v, level):
    if isinstance(v, basestring_compat):
      yield '"' + ''.join(_TranslateToGnChars(v)) + '"'

    elif isinstance(v, bool):
      yield 'true' if v else 'false'

    elif isinstance(v, int):
      yield str(v)

    elif isinstance(v, list):
      yield '['
      for i, item in enumerate(v):
        if i > 0:
          yield ','
        for tok in GenerateTokens(item, level + 1):
          yield tok
      yield ']'

    elif isinstance(v, dict):
      if level > 0:
        yield '{'
      for key in sorted(v):
        if not isinstance(key, basestring_compat):
          raise GNError('Dictionary key is not a string.')
        if not key or key[0].isdigit() or not key.replace('_', '').isalnum():
          raise GNError('Dictionary key is not a valid GN identifier.')
        yield key  # No quotations.
        yield '='
        for tok in GenerateTokens(v[key], level + 1):
          yield tok
      if level > 0:
        yield '}'

    else:  # Not supporting float: Add only when needed.
      raise GNError('Unsupported type when printing to GN.')

  can_start = lambda tok: tok and tok not in ',}]='
  can_end = lambda tok: tok and tok not in ',{[='

  # Adds whitespaces, trying to keep everything (except dicts) in 1 line.
  def PlainGlue(gen):
    prev_tok = None
    for i, tok in enumerate(gen):
      if i > 0:
        if can_end(prev_tok) and can_start(tok):
          yield '\n'  # New dict item.
        elif prev_tok == '[' and tok == ']':
          yield '  '  # Special case for [].
        elif tok != ',':
          yield ' '
      yield tok
      prev_tok = tok

  # Adds whitespaces so non-empty lists can span multiple lines, with indent.
  def PrettyGlue(gen):
    prev_tok = None
    level = 0
    for i, tok in enumerate(gen):
      if i > 0:
        if can_end(prev_tok) and can_start(tok):
          yield '\n' + '  ' * level  # New dict item.
        elif tok == '=' or prev_tok in '=':
          yield ' '  # Separator before and after '=', on same line.
      if tok in ']}':
        level -= 1
      # Exclude '[]' and '{}' cases.
      if int(prev_tok == '[') + int(tok == ']') == 1 or \
         int(prev_tok == '{') + int(tok == '}') == 1:
        yield '\n' + '  ' * level
      yield tok
      if tok in '[{':
        level += 1
      if tok == ',':
        yield '\n' + '  ' * level
      prev_tok = tok

  token_gen = GenerateTokens(value, 0)
  ret = ''.join((PrettyGlue if pretty else PlainGlue)(token_gen))
  # Add terminating '\n' for dict |value| or multi-line output.
  if isinstance(value, dict) or '\n' in ret:
    return ret + '\n'
  return ret


def FromGNString(input_string):
  """Converts the input string from a GN serialized value to Python values.

  For details on supported types see GNValueParser.Parse() below.

  If your GN script did:
    something = [ "file1", "file2" ]
    args = [ "--values=$something" ]
  The command line would look something like:
    --values="[ \"file1\", \"file2\" ]"
  Which when interpreted as a command line gives the value:
    [ "file1", "file2" ]

  You can parse this into a Python list using GN rules with:
    input_values = FromGNValues(options.values)
  Although the Python 'ast' module will parse many forms of such input, it
  will not handle GN escaping properly, nor GN booleans. You should use this
  function instead.


  A NOTE ON STRING HANDLING:

  If you just pass a string on the command line to your Python script, or use
  string interpolation on a string variable, the strings will not be quoted:
    str = "asdf"
    args = [ str, "--value=$str" ]
  Will yield the command line:
    asdf --value=asdf
  The unquoted asdf string will not be valid input to this function, which
  accepts only quoted strings like GN scripts. In such cases, you can just use
  the Python string literal directly.

  The main use cases for this is for other types, in particular lists. When
  using string interpolation on a list (as in the top example) the embedded
  strings will be quoted and escaped according to GN rules so the list can be
  re-parsed to get the same result.
  """
  parser = GNValueParser(input_string)
  return parser.Parse()


def FromGNArgs(input_string):
  """Converts a string with a bunch of gn arg assignments into a Python dict.

  Given a whitespace-separated list of

    <ident> = (integer | string | boolean | <list of the former>)

  gn assignments, this returns a Python dict, i.e.:

    FromGNArgs('foo=true\nbar=1\n') -> { 'foo': True, 'bar': 1 }.

  Only simple types and lists supported; variables, structs, calls
  and other, more complicated things are not.

  This routine is meant to handle only the simple sorts of values that
  arise in parsing --args.
  """
  parser = GNValueParser(input_string)
  return parser.ParseArgs()


def UnescapeGNString(value):
  """Given a string with GN escaping, returns the unescaped string.

  Be careful not to feed with input from a Python parsing function like
  'ast' because it will do Python unescaping, which will be incorrect when
  fed into the GN unescaper.

  Args:
    value: Input string to unescape.
  """
  result = ''
  i = 0
  while i < len(value):
    if value[i] == '\\':
      if i < len(value) - 1:
        next_char = value[i + 1]
        if next_char in ('$', '"', '\\'):
          # These are the escaped characters GN supports.
          result += next_char
          i += 1
        else:
          # Any other backslash is a literal.
          result += '\\'
    else:
      result += value[i]
    i += 1
  return result


def _IsDigitOrMinus(char):
  return char in '-0123456789'


class GNValueParser(object):
  """Duplicates GN parsing of values and converts to Python types.

  Normally you would use the wrapper function FromGNValue() below.

  If you expect input as a specific type, you can also call one of the Parse*
  functions directly. All functions throw GNError on invalid input.
  """

  def __init__(self, string, checkout_root=_CHROMIUM_ROOT):
    self.input = string
    self.cur = 0
    self.checkout_root = checkout_root

  def IsDone(self):
    return self.cur == len(self.input)

  def ReplaceImports(self):
    """Replaces import(...) lines with the contents of the imports.

    Recurses on itself until there are no imports remaining, in the case of
    nested imports.
    """
    lines = self.input.splitlines()
    if not any(line.startswith('import(') for line in lines):
      return
    for line in lines:
      if not line.startswith('import('):
        continue
      regex_match = IMPORT_RE.match(line)
      if not regex_match:
        raise GNError('Not a valid import string: %s' % line)
      import_path = os.path.join(self.checkout_root, regex_match.group(1))
      with open(import_path) as f:
        imported_args = f.read()
      self.input = self.input.replace(line, imported_args)
    # Call ourselves again if we've just replaced an import() with additional
    # imports.
    self.ReplaceImports()


  def _ConsumeWhitespace(self):
    while not self.IsDone() and self.input[self.cur] in ' \t\n':
      self.cur += 1

  def ConsumeCommentAndWhitespace(self):
    self._ConsumeWhitespace()

    # Consume each comment, line by line.
    while not self.IsDone() and self.input[self.cur] == '#':
      # Consume the rest of the comment, up until the end of the line.
      while not self.IsDone() and self.input[self.cur] != '\n':
        self.cur += 1
      # Move the cursor to the next line (if there is one).
      if not self.IsDone():
        self.cur += 1

      self._ConsumeWhitespace()

  def Parse(self):
    """Converts a string representing a printed GN value to the Python type.

    See additional usage notes on FromGNString() above.

    * GN booleans ('true', 'false') will be converted to Python booleans.

    * GN numbers ('123') will be converted to Python numbers.

    * GN strings (double-quoted as in '"asdf"') will be converted to Python
      strings with GN escaping rules. GN string interpolation (embedded
      variables preceded by $) are not supported and will be returned as
      literals.

    * GN lists ('[1, "asdf", 3]') will be converted to Python lists.

    * GN scopes ('{ ... }') are not supported.

    Raises:
      GNError: Parse fails.
    """
    result = self._ParseAllowTrailing()
    self.ConsumeCommentAndWhitespace()
    if not self.IsDone():
      raise GNError("Trailing input after parsing:\n  " + self.input[self.cur:])
    return result

  def ParseArgs(self):
    """Converts a whitespace-separated list of ident=literals to a dict.

    See additional usage notes on FromGNArgs(), above.

    Raises:
      GNError: Parse fails.
    """
    d = {}

    self.ReplaceImports()
    self.ConsumeCommentAndWhitespace()

    while not self.IsDone():
      ident = self._ParseIdent()
      self.ConsumeCommentAndWhitespace()
      if self.input[self.cur] != '=':
        raise GNError("Unexpected token: " + self.input[self.cur:])
      self.cur += 1
      self.ConsumeCommentAndWhitespace()
      val = self._ParseAllowTrailing()
      self.ConsumeCommentAndWhitespace()
      d[ident] = val

    return d

  def _ParseAllowTrailing(self):
    """Internal version of Parse() that doesn't check for trailing stuff."""
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError("Expected input to parse.")

    next_char = self.input[self.cur]
    if next_char == '[':
      return self.ParseList()
    elif next_char == '{':
      return self.ParseScope()
    elif _IsDigitOrMinus(next_char):
      return self.ParseNumber()
    elif next_char == '"':
      return self.ParseString()
    elif self._ConstantFollows('true'):
      return True
    elif self._ConstantFollows('false'):
      return False
    else:
      raise GNError("Unexpected token: " + self.input[self.cur:])

  def _ParseIdent(self):
    ident = ''

    next_char = self.input[self.cur]
    if not next_char.isalpha() and not next_char=='_':
      raise GNError("Expected an identifier: " + self.input[self.cur:])

    ident += next_char
    self.cur += 1

    next_char = self.input[self.cur]
    while next_char.isalpha() or next_char.isdigit() or next_char=='_':
      ident += next_char
      self.cur += 1
      next_char = self.input[self.cur]

    return ident

  def ParseNumber(self):
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Expected number but got nothing.')

    begin = self.cur

    # The first character can include a negative sign.
    if not self.IsDone() and _IsDigitOrMinus(self.input[self.cur]):
      self.cur += 1
    while not self.IsDone() and self.input[self.cur].isdigit():
      self.cur += 1

    number_string = self.input[begin:self.cur]
    if not len(number_string) or number_string == '-':
      raise GNError('Not a valid number.')
    return int(number_string)

  def ParseString(self):
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Expected string but got nothing.')

    if self.input[self.cur] != '"':
      raise GNError('Expected string beginning in a " but got:\n  ' +
                    self.input[self.cur:])
    self.cur += 1  # Skip over quote.

    begin = self.cur
    while not self.IsDone() and self.input[self.cur] != '"':
      if self.input[self.cur] == '\\':
        self.cur += 1  # Skip over the backslash.
        if self.IsDone():
          raise GNError('String ends in a backslash in:\n  ' + self.input)
      self.cur += 1

    if self.IsDone():
      raise GNError('Unterminated string:\n  ' + self.input[begin:])

    end = self.cur
    self.cur += 1  # Consume trailing ".

    return UnescapeGNString(self.input[begin:end])

  def ParseList(self):
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Expected list but got nothing.')

    # Skip over opening '['.
    if self.input[self.cur] != '[':
      raise GNError('Expected [ for list but got:\n  ' + self.input[self.cur:])
    self.cur += 1
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Unterminated list:\n  ' + self.input)

    list_result = []
    previous_had_trailing_comma = True
    while not self.IsDone():
      if self.input[self.cur] == ']':
        self.cur += 1  # Skip over ']'.
        return list_result

      if not previous_had_trailing_comma:
        raise GNError('List items not separated by comma.')

      list_result += [ self._ParseAllowTrailing() ]
      self.ConsumeCommentAndWhitespace()
      if self.IsDone():
        break

      # Consume comma if there is one.
      previous_had_trailing_comma = self.input[self.cur] == ','
      if previous_had_trailing_comma:
        # Consume comma.
        self.cur += 1
        self.ConsumeCommentAndWhitespace()

    raise GNError('Unterminated list:\n  ' + self.input)

  def ParseScope(self):
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Expected scope but got nothing.')

    # Skip over opening '{'.
    if self.input[self.cur] != '{':
      raise GNError('Expected { for scope but got:\n ' + self.input[self.cur:])
    self.cur += 1
    self.ConsumeCommentAndWhitespace()
    if self.IsDone():
      raise GNError('Unterminated scope:\n ' + self.input)

    scope_result = {}
    while not self.IsDone():
      if self.input[self.cur] == '}':
        self.cur += 1
        return scope_result

      ident = self._ParseIdent()
      self.ConsumeCommentAndWhitespace()
      if self.input[self.cur] != '=':
        raise GNError("Unexpected token: " + self.input[self.cur:])
      self.cur += 1
      self.ConsumeCommentAndWhitespace()
      val = self._ParseAllowTrailing()
      self.ConsumeCommentAndWhitespace()
      scope_result[ident] = val

    raise GNError('Unterminated scope:\n ' + self.input)

  def _ConstantFollows(self, constant):
    """Checks and maybe consumes a string constant at current input location.

    Param:
      constant: The string constant to check.

    Returns:
      True if |constant| follows immediately at the current location in the
      input. In this case, the string is consumed as a side effect. Otherwise,
      returns False and the current position is unchanged.
    """
    end = self.cur + len(constant)
    if end > len(self.input):
      return False  # Not enough room.
    if self.input[self.cur:end] == constant:
      self.cur = end
      return True
    return False


def ReadBuildVars(output_directory):
  """Parses $output_directory/build_vars.json into a dict."""
  with open(os.path.join(output_directory, BUILD_VARS_FILENAME)) as f:
    return json.load(f)


def CreateBuildCommand(output_directory):
  """Returns [cmd, -C, output_directory].

  Where |cmd| is one of: siso ninja, ninja, or autoninja.
  """
  suffix = '.bat' if sys.platform.startswith('win32') else ''
  # Prefer the version on PATH, but fallback to known version if PATH doesn't
  # have one (e.g. on bots).
  if not shutil.which(f'autoninja{suffix}'):
    third_party_prefix = os.path.join(_CHROMIUM_ROOT, 'third_party')
    ninja_prefix = os.path.join(third_party_prefix, 'ninja', '')
    siso_prefix = os.path.join(third_party_prefix, 'siso', 'cipd', '')
    # Also - bots configure reclient manually, and so do not use the "auto"
    # wrappers.
    ninja_cmd = [f'{ninja_prefix}ninja{suffix}']
    siso_cmd = [f'{siso_prefix}siso{suffix}', 'ninja']
  else:
    ninja_cmd = [f'autoninja{suffix}']
    siso_cmd = list(ninja_cmd)

  if output_directory and os.path.abspath(output_directory) != os.path.abspath(
      os.curdir):
    ninja_cmd += ['-C', output_directory]
    siso_cmd += ['-C', output_directory]
  siso_deps = os.path.exists(os.path.join(output_directory, '.siso_deps'))
  ninja_deps = os.path.exists(os.path.join(output_directory, '.ninja_deps'))
  if siso_deps and ninja_deps:
    raise Exception('Found both .siso_deps and .ninja_deps in '
                    f'{output_directory}. Not sure which build tool to use. '
                    'Please delete one, or better, run "gn clean".')
  if siso_deps:
    return siso_cmd
  return ninja_cmd
