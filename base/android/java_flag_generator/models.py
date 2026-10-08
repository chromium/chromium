# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Data and error types shared by the java_flag_generator stages."""

import dataclasses
from typing import Iterable, Optional, Tuple


class GeneratorError(Exception):
    """One or more user-facing errors. Each message is printed on its own."""

    def __init__(self, messages: Iterable[str]):
        self.messages = list(messages)
        super().__init__('\n'.join(self.messages))


@dataclasses.dataclass(frozen=True)
class JavaClass:
    package: str
    name: str

    @property
    def full_name(self) -> str:
        return f'{self.package}.{self.name}'


@dataclasses.dataclass(frozen=True)
class FeatureEntry:
    """One `&namespace::kFoo` entry of a kFeaturesExposedToJava array."""
    source_path: str
    line: int
    # Namespace qualifier as written, e.g. "autofill::features::", "::" or "".
    qualifier: str
    identifier: str

    @property
    def written(self) -> str:
        return self.qualifier + self.identifier


@dataclasses.dataclass(frozen=True)
class DefinitionDirective:
    """A `// FEATURE_DEFINITION_FILE: //path/to/file.cc` comment."""
    source_path: str
    line: int
    # The path as written in the comment, e.g. "//path/to/file.cc".
    written_path: str


@dataclasses.dataclass(frozen=True)
class ParsedSource:
    """Everything extracted from one C++ feature list file."""
    path: str
    entries: Tuple[FeatureEntry, ...]
    # Namespace enclosing kFeaturesExposedToJava, used for qualified lookup.
    array_namespace: Tuple[str, ...]
    includes: Tuple[str, ...]
    directives: Tuple[DefinitionDirective, ...]
    # Simple Java class names taken from JNI_<Name>_GetNativeMap functions,
    # sorted. Ignored when feature_map_class_name is set.
    feature_map_names: Tuple[str, ...]


@dataclasses.dataclass(frozen=True)
class FeatureDefinition:
    """A feature definition such as `BASE_FEATURE(kFoo, ...)`."""
    path: str
    line: int
    namespace: Tuple[str, ...]
    identifier: str
    macro: str
    # The feature's string name. None when it can't be determined, in which case
    # `error` explains why. The error is only reported if an entry needs it.
    name: Optional[str]
    error: Optional[str] = None

    @property
    def qualified_name(self) -> str:
        return '::'.join(self.namespace + (self.identifier, ))


@dataclasses.dataclass(frozen=True)
class ResolvedFeature:
    entry: FeatureEntry
    definition: FeatureDefinition
    feature_name: str
    java_class_name: str
    feature_map: JavaClass


@dataclasses.dataclass(frozen=True)
class FeaturesClass:
    """The complete model of the generated Java class."""
    java_class: JavaClass
    # Distinct FeatureMap classes of `features`, in order of first appearance.
    feature_maps: Tuple[JavaClass, ...]
    features: Tuple[ResolvedFeature, ...]
    # Source-absolute ("//...") paths of the C++ feature list files.
    source_paths: Tuple[str, ...]
