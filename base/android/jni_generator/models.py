# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes that are the "M" of MVC."""

import dataclasses
from typing import List
from typing import Optional


@dataclasses.dataclass(frozen=True)
class Param:
  """Describes a param for a method, either java or native."""
  annotations: List[str]
  datatype: str
  name: str


@dataclasses.dataclass(frozen=True)
class JavaClass:
  _fqn: str
  visibility: Optional[str] = None

  def __post_init__(self):
    assert '.' not in self._fqn, f'{self._fqn} should have / and $, but not .'

  def __str__(self):
    return self.full_name_with_dots

  @property
  def name(self):
    return self._fqn.rsplit('/', 1)[-1]

  @property
  def name_with_dots(self):
    return self.name.replace('$', '.')

  @property
  def nested_name(self):
    return self.name.rsplit('$', 1)[-1]

  @property
  def package_with_slashes(self):
    return self._fqn.rsplit('/', 1)[0]

  @property
  def package_with_dots(self):
    return self.package_with_slashes.replace('/', '.')

  @property
  def full_name_with_slashes(self):
    return self._fqn

  @property
  def full_name_with_dots(self):
    return self._fqn.replace('/', '.').replace('$', '.')

  @property
  def is_public(self):
    return self.visibility == 'public'

  def make_prefixed(self, prefix=None):
    if not prefix:
      return self
    prefix = prefix.replace('.', '/')
    return JavaClass(f'{prefix}/{self._fqn}', visibility=self.visibility)

  def make_nested(self, name, visibility=None):
    return JavaClass(f'{self._fqn}${name}', visibility=visibility)
