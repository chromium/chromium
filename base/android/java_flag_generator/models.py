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
class ParamType:
    """The type of the params in a k<Name>ParamsExposedToJava array."""

    # The <Name> in k<Name>ParamsExposedToJava and in the Java
    # FeatureMap.getParamValueAs<Name>() method.
    name: str
    # Java type of the param values.
    java_type: str

    @property
    def array_name(self) -> str:
        return f'k{self.name}ParamsExposedToJava'


PARAM_TYPES = (
    ParamType('Boolean', 'boolean'),
    ParamType('Int', 'int'),
    ParamType('Double', 'double'),
    ParamType('String', 'String'),
)


@dataclasses.dataclass(frozen=True)
class ArrayEntry:
    """One `&namespace::kFoo` entry of a kFeaturesExposedToJava or
    k<Type>ParamsExposedToJava array. Also used for the `&namespace::kFoo`
    feature argument of a param definition.
    """

    source_path: str
    line: int
    # Namespace qualifier as written, e.g. "autofill::features::", "::" or "".
    qualifier: str
    identifier: str

    @property
    def written(self) -> str:
        return self.qualifier + self.identifier


@dataclasses.dataclass(frozen=True)
class Array:
    """A kFeaturesExposedToJava array or a k<Type>ParamsExposedToJava array,
    e.g. kIntParamsExposedToJava.
    """

    # This is None for kFeaturesExposedToJava array.
    param_type: Optional[ParamType]
    # Namespace enclosing the array, used for qualified lookup.
    namespace: Tuple[str, ...]
    entries: Tuple[ArrayEntry, ...]


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
    includes: Tuple[str, ...]
    directives: Tuple[DefinitionDirective, ...]
    # Simple Java class names taken from JNI_<Name>_GetNativeMap functions,
    # sorted. Ignored when feature_map_class_name is set.
    feature_map_names: Tuple[str, ...]
    # The kFeaturesExposedToJava array defined in the file.
    feature_array: Array
    # The k<Type>ParamsExposedToJava arrays defined in the file, in PARAM_TYPES
    # order.
    param_arrays: Tuple[Array, ...] = ()


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
        return '::'.join(self.namespace + (self.identifier,))


@dataclasses.dataclass(frozen=True)
class ParamDefinition:
    """A param definition such as `BASE_FEATURE_PARAM(int, kFoo, &kBar, 0)`."""

    path: str
    line: int
    namespace: Tuple[str, ...]
    identifier: str
    # The param's string name and its feature (`&kBar` above). None when they
    # can't be determined, in which case `error` explains why. The error is only
    # reported if an entry needs it.
    name: Optional[str]
    feature: Optional[ArrayEntry]
    error: Optional[str] = None

    @property
    def qualified_name(self) -> str:
        return '::'.join(self.namespace + (self.identifier,))


@dataclasses.dataclass(frozen=True)
class ResolvedFeature:
    entry: ArrayEntry
    definition: FeatureDefinition
    feature_name: str
    java_class_name: str
    feature_map: JavaClass


@dataclasses.dataclass(frozen=True)
class ResolvedParam:
    param_type: ParamType
    entry: ArrayEntry
    definition: ParamDefinition
    # The name of the param's feature.
    feature_name: str
    param_name: str
    java_class_name: str
    feature_map: JavaClass


@dataclasses.dataclass(frozen=True)
class FeaturesClass:
    """The complete model of the generated Java class."""

    java_class: JavaClass
    # Distinct FeatureMap classes of `features` and `params`, in order of first
    # appearance.
    feature_maps: Tuple[JavaClass, ...]
    features: Tuple[ResolvedFeature, ...]
    params: Tuple[ResolvedParam, ...]
    # Source-absolute ("//...") paths of the C++ feature list files.
    source_paths: Tuple[str, ...]
