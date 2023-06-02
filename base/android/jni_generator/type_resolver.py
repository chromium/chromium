# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import java_lang_classes
import models

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


class TypeResolver:
  """Converts type names to fully qualified names."""
  def __init__(self, java_class):
    self._java_class = java_class
    self.imports = []
    self._nested_classes = []

  def add_import(self, java_class):
    self.imports.append(java_class)

  def add_nested_class(self, java_class):
    self._nested_classes.append(java_class)

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

  def resolve_type(self, type_str):
    """Returns the given type with an "OuterClass." prefix when applicable."""
    array_idx = type_str.find('[')
    if array_idx != -1:
      suffix = type_str[array_idx:]
      type_str = type_str[:array_idx]
    else:
      suffix = ''

    if type_str in _PRIMITIVE_MAP:
      return type_str + suffix
    ret = self._resolve_helper(type_str)
    return models.JavaClass(ret).name_with_dots + suffix

  def _resolve_helper(self, param):
    if '/' in param:
      # Coming from javap, use the fully qualified param directly.
      return param

    if self._java_class.name == param:
      return self._java_class.full_name_with_slashes

    for clazz in self._nested_classes:
      if param in (clazz.name, clazz.nested_name):
        return clazz.full_name_with_slashes

    # Is it from an import? (e.g. referecing Class from import pkg.Class;
    # note that referencing an inner class Inner from import pkg.Class.Inner
    # is not supported).
    for clazz in self.imports:
      if param in (clazz.name, clazz.nested_name):
        return clazz.full_name_with_slashes

    # Is it an inner class from an outer class import? (e.g. referencing
    # Class.Inner from import pkg.Class).
    if '.' in param:
      components = param.split('.')
      outer = '/'.join(components[:-1])
      inner = components[-1]
      for clazz in self.imports:
        if clazz.name == outer:
          return f'{clazz.full_name_with_slashes}${inner}'
      param = param.replace('.', '$')

    # java.lang classes always take priority over types from the same package.
    # To use a type from the same package that has the same name as a java.lang
    # type, it must be explicitly imported.
    if java_lang_classes.contains(param):
      return f'java/lang/{param}'

    # Type not found, falling back to same package as this class.
    return f'{self._java_class.package_with_slashes}/{param}'

  def create_signature(self, params, returns):
    """Returns the JNI signature for the given datatypes."""
    sb = ['"(']
    sb += [self.java_to_jni(p.datatype) for p in params]
    sb += [')']
    sb += [self.java_to_jni(returns)]
    sb += ['"']
    return ''.join(sb)
