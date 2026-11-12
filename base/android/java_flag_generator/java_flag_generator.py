#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a Java class exposing the features in kFeaturesExposedToJava and
the params in k<Type>ParamsExposedToJava.

For each `&namespace::kFoo` in kFeaturesExposedToJava in a C++ feature list,
the generated class gets a nested class `Foo` with the feature's NAME and an
isEnabled() accessor. Similarly, for each `&namespace::kBar` in
kBooleanParamsExposedToJava, kIntParamsExposedToJava,
kDoubleParamsExposedToJava or kStringParamsExposedToJava, it gets a nested
class `Bar` with the param's NAME and a getValue() accessor. Feature and param
names are read from their definitions (see README.md for more details).
"""

import argparse
import pathlib
import sys
import zipfile
from typing import Dict, List, Optional, Tuple

import cpp_lexer
import cpp_parser
import definition_files
from codegen import features_java
import feature_resolver
import models
import naming

_SCRIPT_PATH = pathlib.Path(__file__).resolve()
_SRC_ROOT = _SCRIPT_PATH.parents[3]
sys.path.append(str(_SRC_ROOT / 'build'))
import action_helpers  # noqa: E402
import zip_helpers  # noqa: E402

# Source-relative path, mentioned in the header of the generated file.
_SCRIPT_NAME = _SCRIPT_PATH.relative_to(_SRC_ROOT).as_posix()


class _FileCache:
    """Reads and lexes each file once, caches the content and the feature and
    param definitions in each file, and remembers every path read (for the
    depfile).
    """

    def __init__(self, is_desktop_android: bool):
        self._is_desktop_android = is_desktop_android
        self._files: Dict[str, Tuple[str, str, str]] = {}
        self._feature_definitions: Dict[
            str, List[models.FeatureDefinition]
        ] = {}
        self._param_definitions: Dict[str, List[models.ParamDefinition]] = {}

    def read(self, path: str) -> Tuple[str, str, str]:
        """Returns the text of a file and its code and structure views (see
        cpp_lexer.lex()), all without the code that is not compiled in this
        build (see cpp_lexer.blank_inactive_branches()).
        """
        if path not in self._files:
            with open(path, encoding='utf-8', errors='replace') as f:
                text = f.read()
            code, structure = cpp_lexer.lex(text)
            self._files[path] = cpp_lexer.blank_inactive_branches(
                text, code, structure, self._is_desktop_android
            )
        return self._files[path]

    def feature_definitions(self, path: str) -> List[models.FeatureDefinition]:
        if path not in self._feature_definitions:
            _, code, structure = self.read(path)
            self._feature_definitions[path] = (
                cpp_parser.parse_feature_definitions(path, code, structure)
            )
        return self._feature_definitions[path]

    def param_definitions(self, path: str) -> List[models.ParamDefinition]:
        if path not in self._param_definitions:
            _, code, structure = self.read(path)
            self._param_definitions[path] = cpp_parser.parse_param_definitions(
                path, code, structure
            )
        return self._param_definitions[path]

    @property
    def paths(self) -> List[str]:
        return list(self._files)


def _resolve_source(
    path: str,
    cache: _FileCache,
    ctx: definition_files.PathContext,
    output_class: models.JavaClass,
    feature_map_override: Optional[models.JavaClass],
) -> Tuple[List[models.ResolvedFeature], List[models.ResolvedParam]]:
    text, code, structure = cache.read(path)
    source = cpp_parser.parse_source(path, text, code, structure)
    feature_map = naming.feature_map_for_source(
        source, output_class, feature_map_override
    )
    candidates = definition_files.automatic_candidates(source, ctx)
    directive_files = definition_files.resolve_directives(
        source, candidates, ctx
    )
    searched = candidates + [p for _, p in directive_files]
    feature_definitions_by_file = {
        p: cache.feature_definitions(p) for p in searched
    }
    param_definitions_by_file = {
        p: cache.param_definitions(p) for p in searched
    }
    feature_matches, param_matches = feature_resolver.resolve(
        source,
        feature_definitions_by_file,
        param_definitions_by_file,
        candidates,
        directive_files,
        ctx,
    )
    features = [
        models.ResolvedFeature(
            entry=m.entry,
            definition=m.definition,
            feature_name=m.definition.name,
            java_class_name=naming.nested_class_name(m.entry),
            feature_map=feature_map,
        )
        for m in feature_matches
    ]
    params = [
        models.ResolvedParam(
            param_type=m.param_type,
            entry=m.entry,
            definition=m.definition,
            feature_name=m.feature.name,
            param_name=m.definition.name,
            java_class_name=naming.nested_class_name(m.entry),
            feature_map=feature_map,
        )
        for m in param_matches
    ]
    return features, params


def _build_model(args, cache: _FileCache) -> models.FeaturesClass:
    ctx = definition_files.PathContext(
        source_root=args.source_root, root_gen_dir=args.root_gen_dir
    )
    output_class = naming.parse_java_class_name(args.class_name, '--class-name')
    feature_map_override = None
    if args.feature_map_class_name:
        feature_map_override = naming.parse_java_class_name(
            args.feature_map_class_name, '--feature-map-class-name'
        )

    features = []
    params = []
    errors = []
    for path in args.sources:
        try:
            source_features, source_params = _resolve_source(
                path, cache, ctx, output_class, feature_map_override
            )
        except models.GeneratorError as e:
            errors.extend(e.messages)
            continue
        features.extend(source_features)
        params.extend(source_params)
    if errors:
        raise models.GeneratorError(errors)

    # Cast to a dict to remove duplicates while preserving order.
    feature_maps = list(
        dict.fromkeys(
            [f.feature_map for f in features] + [p.feature_map for p in params]
        )
    )
    features_class = models.FeaturesClass(
        java_class=output_class,
        feature_maps=tuple(feature_maps),
        features=tuple(features),
        params=tuple(params),
        source_paths=tuple(ctx.display(p) for p in args.sources),
    )
    naming.validate(features_class)
    return features_class


def _write_srcjar(path: str, java_class: models.JavaClass, source: str) -> None:
    zip_path = java_class.full_name.replace('.', '/') + '.java'
    with action_helpers.atomic_output(path) as f:
        with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
            zip_helpers.add_to_zip_hermetic(srcjar, zip_path, data=source)


def _parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--class-name',
        required=True,
        help='Fully-qualified name of the generated class.',
    )
    parser.add_argument(
        '--feature-map-class-name',
        help='Fully-qualified name of the FeatureMap subclass, used to query '
        'feature values. Derived from JNI_<Name>_GetNativeMap when omitted.',
    )
    parser.add_argument(
        '--source-root', required=True, help='Path to the source root (//).'
    )
    parser.add_argument(
        '--root-gen-dir',
        required=True,
        help='Path to $root_gen_dir, used to find generated '
        'FEATURE_DEFINITION_FILEs.',
    )
    parser.add_argument(
        '--output-srcjar', required=True, help='Path of the .srcjar to write.'
    )
    parser.add_argument('--depfile', help='Path of the depfile to write.')
    parser.add_argument(
        '--is-desktop-android',
        action='store_true',
        help='Whether BUILDFLAG(IS_DESKTOP_ANDROID) is true in this build.',
    )
    parser.add_argument(
        'sources',
        nargs='+',
        help='C++ files that define kFeaturesExposedToJava, '
        'and optionally k<Type>ParamsExposedToJava.',
    )
    return parser.parse_args(argv)


def main(argv):
    args = _parse_args(argv)
    cache = _FileCache(args.is_desktop_android)
    try:
        features_class = _build_model(args, cache)
    except models.GeneratorError as e:
        for message in e.messages:
            sys.stderr.write(message + '\n')
        return 1

    java_source = features_java.generate(features_class, _SCRIPT_NAME)
    _write_srcjar(args.output_srcjar, features_class.java_class, java_source)
    if args.depfile:
        action_helpers.write_depfile(
            args.depfile, args.output_srcjar, cache.paths
        )
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
