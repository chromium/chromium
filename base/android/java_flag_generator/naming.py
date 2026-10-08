# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Java naming: class_name parsing, FeatureMap derivation, clash checks."""

import re
from typing import Dict, List, Optional

import models

_JAVA_CLASS_NAME_RE = re.compile(r'^((?:[a-z_][A-Za-z0-9_]*\.)+)([A-Z]\w*)$')

# Simple type names referenced by the generated code. A nested class with one
# of these names would shadow it.
_RESERVED_TYPE_NAMES = ('String', 'NullMarked')


def parse_java_class_name(full_name: str, arg_name: str) -> models.JavaClass:
    m = _JAVA_CLASS_NAME_RE.match(full_name)
    if not m:
        raise models.GeneratorError([
            f'error: {arg_name} "{full_name}" is not a fully-qualified Java '
            f'class name such as "org.chromium.foo.FooFeatures".'
        ])
    return models.JavaClass(package=m.group(1)[:-1], name=m.group(2))


def feature_map_for_source(
        source: models.ParsedSource, output_class: models.JavaClass,
        override: Optional[models.JavaClass]) -> models.JavaClass:
    """Returns the FeatureMap class used to query the features of `source`.
    When `override` is set, it is returned without looking at `source`.
    """
    if override:
        return override
    names = source.feature_map_names
    if not names:
        raise models.GeneratorError([
            f'{source.path}: error: Cannot determine the FeatureMap class: no '
            f'JNI_<Name>_GetNativeMap function found. Please set '
            f'feature_map_class_name in the java_flag_generator() target, e.g. '
            f'feature_map_class_name = "{output_class.package}.FooFeatureMap".'
        ])
    if len(names) > 1:
        raise models.GeneratorError([
            f'{source.path}: error: Cannot determine the FeatureMap class: '
            f'found several JNI_<Name>_GetNativeMap functions '
            f'({", ".join(names)}). Set feature_map_class_name in the '
            f'java_flag_generator() target, e.g. '
            f'feature_map_class_name = "{output_class.package}.FooFeatureMap".'
        ])
    return models.JavaClass(output_class.package, names[0])


def nested_class_name(entry: models.FeatureEntry) -> str:
    """kFoo -> Foo.
    This Foo is always a valid Java identifier (see cpp_parser._ENTRY_RE).
    """
    return entry.identifier[1:]


def helper_method_name(feature_map: models.JavaClass,
                       all_maps: List[models.JavaClass]) -> str:
    if len(all_maps) == 1:
        return 'isFeatureEnabled'
    return f'isFeatureEnabledIn{feature_map.name}'


def validate(features_class: models.FeaturesClass) -> None:
    errors = []
    outer = features_class.java_class.name

    maps_by_simple_name: Dict[str, models.JavaClass] = {}
    for feature_map in features_class.feature_maps:
        other = maps_by_simple_name.setdefault(feature_map.name, feature_map)
        if other != feature_map:
            errors.append(
                f'error: FeatureMap classes {other.full_name} and '
                f'{feature_map.full_name} have the same simple name.')
        if feature_map.name == outer:
            errors.append(
                f'error: The generated class {outer} has the same name as '
                f'its FeatureMap class {feature_map.full_name}.')

    reserved = set(_RESERVED_TYPE_NAMES) | set(maps_by_simple_name)
    by_class_name: Dict[str, models.ResolvedFeature] = {}
    by_feature_name: Dict[str, models.ResolvedFeature] = {}
    for feature in features_class.features:
        entry = feature.entry
        name = feature.java_class_name
        where = f'{entry.source_path}:{entry.line}'
        if name == outer:
            errors.append(
                f'{where}: error: Nested class "{name}" would have the '
                f'same name as the generated class.')
        elif name in reserved:
            errors.append(f'{where}: error: Nested class "{name}" would '
                          f'shadow a type used by the generated class.')

        first = by_class_name.setdefault(name, feature)
        if first is not feature:
            errors.append(
                f'{where}: error: "{entry.written}" and '
                f'"{first.entry.written}" ({first.entry.source_path}:'
                f'{first.entry.line}) both map to the nested class '
                f'"{name}".')
            continue
        first = by_feature_name.setdefault(feature.feature_name, feature)
        if first is not feature:
            errors.append(
                f'{where}: error: "{entry.written}" and '
                f'"{first.entry.written}" ({first.entry.source_path}:'
                f'{first.entry.line}) both have the feature name '
                f'"{feature.feature_name}".')
    if errors:
        raise models.GeneratorError(errors)
