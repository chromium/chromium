#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a Java class exposing the features in kFeaturesExposedToJava.

For each `&namespace::kFoo` in kFeaturesExposedToJava in a C++ feature list,
the generated class gets a nested class `Foo` with the feature's NAME and an
isEnabled() accessor. Feature names are read from the features' definitions
(see README.md for more details).
"""

import argparse
import pathlib
import sys
import zipfile
from typing import Dict, List, Optional

import cpp_parser
import definition_files
from codegen import features_java
import feature_resolver
import models
import naming

_SCRIPT_PATH = pathlib.Path(__file__).resolve()
_SRC_ROOT = _SCRIPT_PATH.parents[3]
sys.path.append(str(_SRC_ROOT / 'build'))
import action_helpers  # pylint: disable=wrong-import-position
import zip_helpers  # pylint: disable=wrong-import-position

# Source-relative path, mentioned in the header of the generated file.
_SCRIPT_NAME = _SCRIPT_PATH.relative_to(_SRC_ROOT).as_posix()


class _FileCache:
    """Reads each file once, caches the content and the feature definitions in
    each file, and remembers every path read (for the depfile).
    """

    def __init__(self):
        self._texts: Dict[str, str] = {}
        self._definitions: Dict[str, List[models.FeatureDefinition]] = {}

    def read(self, path: str) -> str:
        if path not in self._texts:
            with open(path, encoding='utf-8', errors='replace') as f:
                self._texts[path] = f.read()
        return self._texts[path]

    def definitions(self, path: str) -> List[models.FeatureDefinition]:
        if path not in self._definitions:
            self._definitions[path] = cpp_parser.parse_definitions(
                path, self.read(path))
        return self._definitions[path]

    @property
    def paths(self) -> List[str]:
        return list(self._texts)


def _resolve_source(
        path: str, cache: _FileCache, ctx: definition_files.PathContext,
        output_class: models.JavaClass,
        feature_map_override: Optional[models.JavaClass]
) -> List[models.ResolvedFeature]:
    source = cpp_parser.parse_source(path, cache.read(path))
    feature_map = naming.feature_map_for_source(source, output_class,
                                                feature_map_override)
    candidates = definition_files.automatic_candidates(source, ctx)
    directive_files = definition_files.resolve_directives(
        source, candidates, ctx)
    definitions_by_file = {
        p: cache.definitions(p)
        for p in candidates + [p for _, p in directive_files]
    }
    matches = feature_resolver.resolve(source, definitions_by_file, candidates,
                                       directive_files, ctx)
    return [
        models.ResolvedFeature(entry=entry,
                               definition=definition,
                               feature_name=definition.name,
                               java_class_name=naming.nested_class_name(entry),
                               feature_map=feature_map)
        for entry, definition in matches
    ]


def _build_model(args, cache: _FileCache) -> models.FeaturesClass:
    ctx = definition_files.PathContext(source_root=args.source_root,
                                       root_gen_dir=args.root_gen_dir)
    output_class = naming.parse_java_class_name(args.class_name,
                                                '--class-name')
    feature_map_override = None
    if args.feature_map_class_name:
        feature_map_override = naming.parse_java_class_name(
            args.feature_map_class_name, '--feature-map-class-name')

    features = []
    errors = []
    for path in args.sources:
        try:
            features.extend(
                _resolve_source(path, cache, ctx, output_class,
                                feature_map_override))
        except models.GeneratorError as e:
            errors.extend(e.messages)
    if errors:
        raise models.GeneratorError(errors)

    # Cast to a dict to remove duplicates while preserving order.
    feature_maps = list(dict.fromkeys(f.feature_map for f in features))
    features_class = models.FeaturesClass(
        java_class=output_class,
        feature_maps=tuple(feature_maps),
        features=tuple(features),
        source_paths=tuple(ctx.display(p) for p in args.sources))
    naming.validate(features_class)
    return features_class


def _write_srcjar(path: str, java_class: models.JavaClass,
                  source: str) -> None:
    zip_path = java_class.full_name.replace('.', '/') + '.java'
    with action_helpers.atomic_output(path) as f:
        with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
            zip_helpers.add_to_zip_hermetic(srcjar, zip_path, data=source)


def _parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--class-name',
                        required=True,
                        help='Fully-qualified name of the generated class.')
    parser.add_argument(
        '--feature-map-class-name',
        help='Fully-qualified name of the FeatureMap subclass, used to query '
        'feature values. Derived from JNI_<Name>_GetNativeMap when omitted.')
    parser.add_argument('--source-root',
                        required=True,
                        help='Path to the source root (//).')
    parser.add_argument('--root-gen-dir',
                        required=True,
                        help='Path to $root_gen_dir, used to find generated '
                        'FEATURE_DEFINITION_FILEs.')
    parser.add_argument('--output-srcjar',
                        required=True,
                        help='Path of the .srcjar to write.')
    parser.add_argument('--depfile', help='Path of the depfile to write.')
    parser.add_argument('sources',
                        nargs='+',
                        help='C++ files that define kFeaturesExposedToJava.')
    return parser.parse_args(argv)


def main(argv):
    args = _parse_args(argv)
    cache = _FileCache()
    try:
        features_class = _build_model(args, cache)
    except models.GeneratorError as e:
        for message in e.messages:
            sys.stderr.write(message + '\n')
        return 1

    java_source = features_java.generate(features_class, _SCRIPT_NAME)
    _write_srcjar(args.output_srcjar, features_class.java_class, java_source)
    if args.depfile:
        action_helpers.write_depfile(args.depfile, args.output_srcjar,
                                     cache.paths)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
