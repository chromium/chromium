# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Refactors BUILD.gn files for our Annotation Processor -> .srcjar migration.

1) Finds all generate_jni() targets
2) Finds all android_library() targets with that use ":jni_processor"
3) Compares lists of sources between them
4) Removes the annotation_processor_deps entry
5) Adds the generate_jni target as a srcjar_dep
6) Updates visibility of generate_jni to allow the dep

This script has already done its job, but is left as an example of using gn_ast.
"""

import argparse
import sys

import gn_ast

_PROCESSOR_DEP = '//base/android/jni_generator:jni_processor'


class RefactorException(Exception):
    pass


def find_processor_assignment(target):
    for assignment in target.block.find_assignments(
            'annotation_processor_deps'):
        processors = assignment.list_value.literals
        if _PROCESSOR_DEP in processors:
            return assignment
    return None


def find_all_sources(target, build_file):
    ret = []

    def helper(assignments):
        for assignment in assignments:
            if assignment.operation not in ('=', '+='):
                raise RefactorException(
                    f'{target.name}: sources has a {assignment.operation}.')

            value = assignment.value
            if value.is_identifier():
                helper(build_file.block.find_assignments(value.node_value))
            elif not value.is_list():
                raise RefactorException(f'{target.name}: sources not a list.')
            else:
                ret.extend(value.literals)

    helper(target.block.find_assignments('sources'))
    return ret


def find_matching_jni_target(library_target, jni_target_to_sources,
                             build_file):
    all_sources = set(find_all_sources(library_target, build_file))
    matches = []
    for jni_target_name, jni_sources in jni_target_to_sources.items():
        if all(s in all_sources for s in jni_sources):
            matches.append(jni_target_name)
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise RefactorException(
            f'{library_target.name}: Matched multiple generate_jni().')
    if jni_target_to_sources:
        raise RefactorException(
            f'{library_target.name}: No matching generate_jni().')
    raise RefactorException('No sources found for generate_jni().')


def fix_visibility(target):
    for assignment in target.block.find_assignments('visibility'):
        if not assignment.value.is_list():
            continue
        list_value = assignment.list_value
        for value in list(list_value.literals):
            if value.startswith(':'):
                list_value.remove_literal(value)
        list_value.add_literal(':*')


def refactor(lib_target, jni_target):
    assignments = lib_target.block.find_assignments('srcjar_deps')
    srcjar_deps = assignments[0] if assignments else None
    if srcjar_deps is None:
        srcjar_deps = gn_ast.AssignmentWrapper.create_list('srcjar_deps')
        first_source_assignment = lib_target.block.find_assignments(
            'sources')[0]
        lib_target.block.add_child(srcjar_deps, before=first_source_assignment)
    elif not srcjar_deps.value.is_list():
        raise RefactorException(
            f'{lib_target.name}: srcjar_deps is not a list.')
    srcjar_deps.list_value.add_literal(f':{jni_target.name}')

    processor_assignment = find_processor_assignment(lib_target)
    processors = processor_assignment.list_value.literals
    if len(processors) == 1:
        lib_target.block.remove_child(processor_assignment.node)
    else:
        processor_assignment.list_value.remove_literal(_PROCESSOR_DEP)

    fix_visibility(jni_target)


def analyze(build_file):
    targets = build_file.targets
    jni_targets = [t for t in targets if t.type == 'generate_jni']
    lib_targets = [t for t in targets if find_processor_assignment(t)]

    if len(jni_targets) == 0 and len(lib_targets) == 0:
        return
    # Match up target when there are only one, even when targets use variables
    # for list values.
    if len(jni_targets) == 1 and len(lib_targets) == 1:
        refactor(lib_targets[0], jni_targets[0])
        return

    jni_target_to_sources = {
        t.name: find_all_sources(t, build_file)
        for t in jni_targets
    }
    for lib_target in lib_targets:
        jni_target_name = find_matching_jni_target(lib_target,
                                                   jni_target_to_sources,
                                                   build_file)
        jni_target = build_file.targets_by_name[jni_target_name]
        refactor(lib_target, jni_target)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('path')
    args = parser.parse_args()
    try:
        build_file = gn_ast.BuildFile.from_file(args.path)
        analyze(build_file)
        if build_file.write_changes():
            print(f'{args.path}: Changes applied.')
        else:
            print(f'{args.path}: No changes necessary.')
    except RefactorException as e:
        print(f'{args.path}: {e}')
        sys.exit(1)
    except Exception:
        print('Failure on', args.path)
        raise


if __name__ == '__main__':
    main()
