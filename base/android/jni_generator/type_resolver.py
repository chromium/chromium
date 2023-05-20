# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import java_lang_classes

_PRIMITIVE_MAP = {
    'int': 'I',
    'boolean': 'Z',
    'char': 'C',
    'short': 'S',
    'long': 'J',
    'double': 'D',
    'float': 'F',
    'byte': 'B',
    'void': 'V',
}

_IMPORT_REGEX = re.compile(r'^import\s+(\S+?);', flags=re.MULTILINE)
_INNER_TYPES_REGEX = re.compile(r'(?:class|interface|enum)\s+?(\w+?)\W')


class TypeResolver:
  """Converts type names to fully qualified names."""
  def __init__(self, fully_qualified_class):
    self._fully_qualified_class = fully_qualified_class
    self._package = '/'.join(fully_qualified_class.split('/')[:-1])
    self._imports = []
    self._inner_classes = []

  def parse_imports_and_nested_types(self, contents):
    names = _IMPORT_REGEX.findall(contents)
    self._imports.extend(
        n.replace('.', '/') for n in names if not n.endswith('*'))

    for name in _INNER_TYPES_REGEX.findall(contents):
      if not self._fully_qualified_class.endswith(name):
        self._inner_classes.append(f'{self._fully_qualified_class}${name}')

  def java_to_jni(self, param):
    """Converts a java param into a JNI signature type."""
    prefix = ''
    # Array?
    while param[-2:] == '[]':
      prefix += '['
      param = param[:-2]
    # Generic?
    if '<' in param:
      param = param[:param.index('<')]
    primitive_letter = _PRIMITIVE_MAP.get(param)
    if primitive_letter:
      return f'{prefix}{primitive_letter}'

    type_name = self._resolve_helper(param)
    return f'{prefix}L{type_name};'

  def _resolve_helper(self, param):
    if '/' in param:
      # Coming from javap, use the fully qualified param directly.
      return param

    for qualified_name in ([self._fully_qualified_class] + self._inner_classes):
      if (qualified_name.endswith('/' + param)
          or qualified_name.endswith('$' + param.replace('.', '$'))
          or qualified_name == param):
        return qualified_name

    # Is it from an import? (e.g. referecing Class from import pkg.Class;
    # note that referencing an inner class Inner from import pkg.Class.Inner
    # is not supported).
    for qualified_name in self._imports:
      if qualified_name.endswith('/' + param):
        # Ensure it's not an inner class.
        components = qualified_name.split('/')
        if len(components) > 2 and components[-2][0].isupper():
          raise SyntaxError('Inner class (%s) can not be imported '
                            'and used by JNI (%s). Please import the outer '
                            'class and use Outer.Inner instead.' %
                            (qualified_name, param))
        return qualified_name

    # Is it an inner class from an outer class import? (e.g. referencing
    # Class.Inner from import pkg.Class).
    if '.' in param:
      components = param.split('.')
      outer = '/'.join(components[:-1])
      inner = components[-1]
      for qualified_name in self._imports:
        if qualified_name.endswith('/' + outer):
          return f'{qualified_name}${inner}'
      param = param.replace('.', '$')

    # java.lang classes always take priority over types from the same package.
    # To use a type from the same package that has the same name as a java.lang
    # type, it must be explicitly imported.
    if java_lang_classes.contains(param):
      return f'java/lang/{param}'

    # Type not found, falling back to same package as this class.
    return f'{self._package}/{param}'

  def create_signature(self, params, returns):
    """Returns the JNI signature for the given datatypes."""
    sb = ['"(']
    sb += [self.java_to_jni(p.datatype) for p in params]
    sb += [')']
    sb += [self.java_to_jni(returns)]
    sb += ['"']
    return ''.join(sb)
