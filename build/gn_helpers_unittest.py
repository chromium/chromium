# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import sys
import textwrap
import unittest

import gn_helpers


class UnitTest(unittest.TestCase):
  def test_ToGNString(self):
    test_cases = [
        (42, '42', '42'), ('foo', '"foo"', '"foo"'), (True, 'true', 'true'),
        (False, 'false', 'false'), ('', '""', '""'),
        ('\\$"$\\', '"\\\\\\$\\"\\$\\\\"', '"\\\\\\$\\"\\$\\\\"'),
        (' \t\r\n', '" $0x09$0x0D$0x0A"', '" $0x09$0x0D$0x0A"'),
        (u'\u2713', '"$0xE2$0x9C$0x93"', '"$0xE2$0x9C$0x93"'),
        ([], '[  ]', '[]'), ([1], '[ 1 ]', '[\n  1\n]\n'),
        ([3, 1, 4, 1], '[ 3, 1, 4, 1 ]', '[\n  3,\n  1,\n  4,\n  1\n]\n'),
        (['a', True, 2], '[ "a", true, 2 ]', '[\n  "a",\n  true,\n  2\n]\n'),
        ({
            'single': 'item'
        }, 'single = "item"\n', 'single = "item"\n'),
        ({
            'kEy': 137,
            '_42A_Zaz_': [False, True]
        }, '_42A_Zaz_ = [ false, true ]\nkEy = 137\n',
         '_42A_Zaz_ = [\n  false,\n  true\n]\nkEy = 137\n'),
        ([1, 'two',
          ['"thr,.$\\', True, False, [],
           u'(\u2713)']], '[ 1, "two", [ "\\"thr,.\\$\\\\", true, false, ' +
         '[  ], "($0xE2$0x9C$0x93)" ] ]', '''[
  1,
  "two",
  [
    "\\"thr,.\\$\\\\",
    true,
    false,
    [],
    "($0xE2$0x9C$0x93)"
  ]
]
'''),
        ({
            's': 'foo',
            'n': 42,
            'b': True,
            'a': [3, 'x']
        }, 'a = [ 3, "x" ]\nb = true\nn = 42\ns = "foo"\n',
         'a = [\n  3,\n  "x"\n]\nb = true\nn = 42\ns = "foo"\n'),
        (
            [[[], [[]]], []],
            '[ [ [  ], [ [  ] ] ], [  ] ]',
            '[\n  [\n    [],\n    [\n      []\n    ]\n  ],\n  []\n]\n',
        ),
        (
            [{
                'a': 1,
                'c': {
                    'z': 8
                },
                'b': []
            }],
            '[ { a = 1\nb = [  ]\nc = { z = 8 } } ]\n',
            '[\n  {\n    a = 1\n    b = []\n    c = {\n' +
            '      z = 8\n    }\n  }\n]\n',
        )
    ]
    for obj, exp_ugly, exp_pretty in test_cases:
      out_ugly = gn_helpers.ToGNString(obj)
      self.assertEqual(exp_ugly, out_ugly)
      out_pretty = gn_helpers.ToGNString(obj, pretty=True)
      self.assertEqual(exp_pretty, out_pretty)

  def test_UnescapeGNString(self):
    # Backslash followed by a \, $, or " means the folling character without
    # the special meaning. Backslash followed by everything else is a literal.
    self.assertEqual(
        gn_helpers.UnescapeGNString('\\as\\$\\\\asd\\"'),
        '\\as$\\asd"')

  def test_FromGNString(self):
    self.assertEqual(
        gn_helpers.FromGNString('[1, -20, true, false,["as\\"", []]]'),
        [ 1, -20, True, False, [ 'as"', [] ] ])

    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('123 456')
      parser.Parse()

  def test_ParseBool(self):
    parser = gn_helpers.GNValueParser('true')
    self.assertEqual(parser.Parse(), True)

    parser = gn_helpers.GNValueParser('false')
    self.assertEqual(parser.Parse(), False)

  def test_ParseNumber(self):
    parser = gn_helpers.GNValueParser('123')
    self.assertEqual(parser.ParseNumber(), 123)

    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('')
      parser.ParseNumber()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('a123')
      parser.ParseNumber()

  def test_ParseString(self):
    parser = gn_helpers.GNValueParser('"asdf"')
    self.assertEqual(parser.ParseString(), 'asdf')

    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('')  # Empty.
      parser.ParseString()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('asdf')  # Unquoted.
      parser.ParseString()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('"trailing')  # Unterminated.
      parser.ParseString()

  def test_ParseList(self):
    parser = gn_helpers.GNValueParser('[1,]')  # Optional end comma OK.
    self.assertEqual(parser.ParseList(), [ 1 ])

    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('')  # Empty.
      parser.ParseList()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('asdf')  # No [].
      parser.ParseList()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('[1, 2')  # Unterminated
      parser.ParseList()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('[1 2]')  # No separating comma.
      parser.ParseList()

  def test_ParseScope(self):
    parser = gn_helpers.GNValueParser('{a = 1}')
    self.assertEqual(parser.ParseScope(), {'a': 1})

    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('')  # Empty.
      parser.ParseScope()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('asdf')  # No {}.
      parser.ParseScope()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('{a = 1')  # Unterminated.
      parser.ParseScope()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('{"a" = 1}')  # Not identifier.
      parser.ParseScope()
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser('{a = }')  # No value.
      parser.ParseScope()

  def test_FromGNArgs(self):
    # Booleans and numbers should work; whitespace is allowed works.
    self.assertEqual(gn_helpers.FromGNArgs('foo = true\nbar = 1\n'),
                     {'foo': True, 'bar': 1})

    # Whitespace is not required; strings should also work.
    self.assertEqual(gn_helpers.FromGNArgs('foo="bar baz"'),
                     {'foo': 'bar baz'})

    # Comments should work (and be ignored).
    gn_args_lines = [
        '# Top-level comment.',
        'foo = true',
        'bar = 1  # In-line comment followed by whitespace.',
        ' ',
        'baz = false',
    ]
    self.assertEqual(gn_helpers.FromGNArgs('\n'.join(gn_args_lines)), {
        'foo': True,
        'bar': 1,
        'baz': False
    })

    # Lists should work.
    self.assertEqual(gn_helpers.FromGNArgs('foo=[1, 2, 3]'),
                     {'foo': [1, 2, 3]})

    # Empty strings should return an empty dict.
    self.assertEqual(gn_helpers.FromGNArgs(''), {})
    self.assertEqual(gn_helpers.FromGNArgs(' \n '), {})

    # Comments should work everywhere (and be ignored).
    gn_args_lines = [
        '# Top-level comment.',
        '',
        '# Variable comment.',
        'foo = true',
        'bar = [',
        '    # Value comment in list.',
        '    1,',
        '    2,',
        ']',
        '',
        'baz # Comment anywhere, really',
        '  = # also here',
        '    4',
    ]
    self.assertEqual(gn_helpers.FromGNArgs('\n'.join(gn_args_lines)), {
        'foo': True,
        'bar': [1, 2],
        'baz': 4
    })

    # Scope should be parsed, even empty ones.
    gn_args_lines = [
        'foo = {',
        '  a = 1',
        '  b = [',
        '    { },',
        '    {',
        '      c = 1',
        '    },',
        '  ]',
        '}',
    ]
    self.assertEqual(gn_helpers.FromGNArgs('\n'.join(gn_args_lines)),
                     {'foo': {
                         'a': 1,
                         'b': [
                             {},
                             {
                                 'c': 1,
                             },
                         ]
                     }})

    # Non-identifiers should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      gn_helpers.FromGNArgs('123 = true')

    # References to other variables should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      gn_helpers.FromGNArgs('foo = bar')

    # References to functions should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      gn_helpers.FromGNArgs('foo = exec_script("//build/baz.py")')

    # Underscores in identifiers should work.
    self.assertEqual(gn_helpers.FromGNArgs('_foo = true'),
                     {'_foo': True})
    self.assertEqual(gn_helpers.FromGNArgs('foo_bar = true'),
                     {'foo_bar': True})
    self.assertEqual(gn_helpers.FromGNArgs('foo_=true'),
                     {'foo_': True})

  def test_ReplaceImports(self):
    # Should be a no-op on args inputs without any imports.
    parser = gn_helpers.GNValueParser(
        textwrap.dedent("""
        some_arg1 = "val1"
        some_arg2 = "val2"
    """))
    parser.ReplaceImports()
    self.assertEqual(
        parser.input,
        textwrap.dedent("""
        some_arg1 = "val1"
        some_arg2 = "val2"
    """))

    # A single "import(...)" line should be replaced with the contents of the
    # file being imported.
    parser = gn_helpers.GNValueParser(
        textwrap.dedent("""
        some_arg1 = "val1"
        import("//some/args/file.gni")
        some_arg2 = "val2"
    """))
    fake_import = 'some_imported_arg = "imported_val"'
    builtin_var = '__builtin__' if sys.version_info.major < 3 else 'builtins'
    open_fun = '{}.open'.format(builtin_var)
    with mock.patch(open_fun, mock.mock_open(read_data=fake_import)):
      parser.ReplaceImports()
    self.assertEqual(
        parser.input,
        textwrap.dedent("""
        some_arg1 = "val1"
        some_imported_arg = "imported_val"
        some_arg2 = "val2"
    """))

    # No trailing parenthesis should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser(
          textwrap.dedent('import("//some/args/file.gni"'))
      parser.ReplaceImports()

    # No double quotes should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser(
          textwrap.dedent('import(//some/args/file.gni)'))
      parser.ReplaceImports()

    # A path that's not source absolute should raise an exception.
    with self.assertRaises(gn_helpers.GNError):
      parser = gn_helpers.GNValueParser(
          textwrap.dedent('import("some/relative/args/file.gni")'))
      parser.ReplaceImports()


if __name__ == '__main__':
  unittest.main()
