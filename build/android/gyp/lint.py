#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs Android's lint tool."""

import argparse
import functools
import logging
import os
import platform
import re
import shutil
import sys
import time
from xml.dom import minidom
from xml.etree import ElementTree

from util import build_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_LINT_MD_URL = 'https://chromium.googlesource.com/chromium/src/+/main/build/android/docs/lint.md'

# lint-graalvm is built for linux-x64 only.
_IS_LINUX_X64 = sys.platform == 'linux' and platform.machine() == 'x86_64'

# These checks are not useful for chromium.
_DISABLED_ALWAYS = [
    "AppCompatResource",  # Lint does not correctly detect our appcompat lib.
    "AppLinkUrlError",  # As a browser, we have intent filters without a host.
    "Assert",  # R8 --force-enable-assertions is used to enable java asserts.
    "InflateParams",  # Null is ok when inflating views for dialogs.
    # Android apps are associated with domains of the same owner. Chrome uses
    # the Credential Manager API to support filling *any* site with a third
    # party password manager. Therefore, the list of sign-in domains would be
    # infinite and this warning must be suppressed.
    "CredManMissingDal",
    "InlinedApi",  # Constants are copied so they are always available.
    "LintBaseline",  # Don't warn about using baseline.xml files.
    "LintBaselineFixed",  # We dont care if baseline.xml has unused entries.
    "MissingInflatedId",  # False positives https://crbug.com/1394222
    "MissingApplicationIcon",  # False positive for non-production targets.
    "MissingVersion",  # We set versions via aapt2, which runs after lint.
    "NetworkSecurityConfig",  # Breaks on library certificates b/269783280.
    "ObsoleteLintCustomCheck",  # We have no control over custom lint checks.
    "OldTargetApi",  # We sometimes need targetSdkVersion to not be latest.
    "PrivateResource",  # Triggers on our own R.java files.
    "StringFormatCount",  # Has false-positives.
    "SwitchIntDef",  # Many C++ enums are not used at all in java.
    "ThreadConstraint",  # Disabled to avoid false positives (b/496928954).
    "StaticInitializerThreadConstraint",  # Same as above (b/496928954).
    # Report blocking calls from code annotated as non-blocking. Chromium has
    # no such annotations, so they report nothing. They also crash partial
    # analysis in lint 32.6 (NPE in getPartialResultDir) unless every
    # JoinEffectDetector issue is off.
    "BlockingMethod",
    "BlockingStaticInitializer",
    "Typos",  # Strings are committed in English first and later translated.
    "VisibleForTests",  # Does not recognize "ForTesting" methods.
    "UniqueConstants",  # Chromium enums allow aliases.
    "UnusedAttribute",  # Chromium apks have various minSdkVersion values.
    "UnusedTranslation",  # Triggers from .aar files with extra translations.
]

# Checks that need the whole app to decide.
_DISABLED_IN_LIBRARIES = [
    # Report a usage only when the rest of the project lacks the alternative
    # (showAsAction="ifRoom", getDefaultUncaughtExceptionHandler()), which a
    # library analysis cannot see. The app analysis still runs them.
    "AlwaysShowAction",
    "DefaultUncaughtExceptionDelegation",
]

# Library analysis and annotation extraction run many small JVMs in parallel.
# Each would otherwise size its GC and JIT thread pools for every core of the
# machine.
_PER_LIBRARY_JVM_FLAGS = ['-XX:+UseSerialGC', '-XX:ActiveProcessorCount=2']

_RES_ZIP_DIR = 'RESZIPS'
_SRCJAR_DIR = 'SRCJARS'

# Placeholder for the source root in the <ignore regexp="..."> entries of
# lint-suppressions.xml files. See _SourceRootRegexp() and
# //build/android/docs/lint.md.
_SRC_PLACEHOLDER = '$SRC'


def _SrcRelative(path):
    """Returns relative path to top-level src dir."""
    return os.path.relpath(path, build_utils.DIR_SOURCE_ROOT)


def _GenerateProjectFile(
    android_manifest,
    android_sdk_root,
    cache_dir,
    partials_dir,
    sources=None,
    classpath=None,
    srcjar_sources=None,
    resource_sources=None,
    aars=None,
    android_sdk_version=None,
    baseline_path=None,
    aar_dirs=None,
    library=False,
    annotation_zips=None,
):
    project = ElementTree.Element('project')
    root = ElementTree.SubElement(project, 'root')
    # Run lint from output directory: crbug.com/1115594
    root.set('dir', os.getcwd())
    sdk = ElementTree.SubElement(project, 'sdk')
    # Lint requires that the sdk path be an absolute path.
    sdk.set('dir', os.path.abspath(android_sdk_root))
    if baseline_path is not None:
        baseline = ElementTree.SubElement(project, 'baseline')
        baseline.set('file', baseline_path)
    cache = ElementTree.SubElement(project, 'cache')
    cache.set('dir', cache_dir)
    main_module = ElementTree.SubElement(project, 'module')
    # Library modules are referred to by name from the app's report step.
    main_module.set('name', _ModuleName(partials_dir) if library else 'main')
    main_module.set('android', 'true')
    main_module.set('library', 'true' if library else 'false')
    # Required to make lint-resources.xml be written to a per-target path.
    # https://crbug.com/1515070 and b/324598620
    main_module.set('partial-results-dir', partials_dir)
    if android_sdk_version:
        main_module.set('compile_sdk_version', android_sdk_version)
    # Cache task has no manifest.
    if android_manifest:
        manifest = ElementTree.SubElement(main_module, 'merged-manifest')
        manifest.set('file', android_manifest)
    if srcjar_sources:
        srcjar_sources = sorted(set(srcjar_sources))  # Ensure these are unique.
        for srcjar_file in srcjar_sources:
            src = ElementTree.SubElement(main_module, 'src')
            src.set('file', srcjar_file)
            # Cannot add generated="true" since then lint does not scan them, and
            # we get "UnusedResources" lint errors when resources are used only by
            # generated files.
    if sources:
        sources = sorted(set(sources))  # Ensure these are unique.
        for source in sources:
            src = ElementTree.SubElement(main_module, 'src')
            src.set('file', source)
            # Cannot set test="true" since we sometimes put Test.java files beside
            # non-test files, which lint does not allow:
            # "Test sources cannot be in the same source root as production files"
    if classpath:
        # The classpath entries should already be unique and order matters.
        for file_path in classpath:
            classpath_element = ElementTree.SubElement(main_module, 'classpath')
            classpath_element.set('file', file_path)
    if resource_sources:
        resource_sources = sorted(
            set(resource_sources)
        )  # Ensure these are unique.
        for resource_file in resource_sources:
            resource = ElementTree.SubElement(main_module, 'resource')
            resource.set('file', resource_file)
    if aars:
        aar_dirs = aar_dirs or {}
        aars = sorted(set(aars))  # Ensure these are unique.
        for aar in aars:
            lint = ElementTree.SubElement(main_module, 'aar')
            lint.set('file', aar)
            if aar_dir := aar_dirs.get(aar):
                # Already unpacked by the build, so lint need not unzip it.
                lint.set('extracted', os.path.abspath(aar_dir))
                # Lint reads an extracted aar's annotations.zip (typedefs and
                # other external annotations of its classes) only when listed.
                annotations = os.path.join(aar_dir, 'annotations.zip')
                if os.path.exists(annotations):
                    ElementTree.SubElement(main_module, 'annotations').set(
                        'file', os.path.abspath(annotations)
                    )
    for path in annotation_zips or []:
        # Typedefs (@IntDef and friends) of dependencies. They have SOURCE
        # retention, so they are missing from the header jars on the classpath.
        # Libraries without any have an empty file instead of a zip.
        if not os.path.getsize(path):
            continue
        ElementTree.SubElement(main_module, 'annotations').set(
            'file', os.path.abspath(path)
        )
    return project


def _ModuleName(partials_dir):
    # Module names must be the same when analyzing a module and when reporting
    # on it, and must not name a directory in the output directory, since lint
    # uses <root>/<name> as the module's directory when it exists.
    return 'lint_module__' + os.path.normpath(partials_dir).replace(
        os.sep, '__'
    )


def _AddDepModule(project, main_module, name, partials_dir):
    ElementTree.SubElement(main_module, 'dep').set('module', name)
    module = ElementTree.SubElement(project, 'module')
    module.set('name', name)
    module.set('android', 'true')
    module.set('library', 'true')
    module.set('partial-results-dir', partials_dir)


_LIBRARY_MANIFEST = '''<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="org.chromium.lint.library">
  <uses-sdk android:minSdkVersion="{min_sdk}" android:targetSdkVersion="{target_sdk}"/>
</manifest>
'''


def _WriteLibraryManifest(path, min_sdk_version, target_sdk_version):
    # Without a manifest, lint treats library modules as targetSdkVersion=1,
    # which silently turns off every check gated on targetSdkVersion.
    build_utils.MakeDirectory(os.path.dirname(path))
    with open(path, 'w', encoding='utf-8') as f:
        f.write(
            _LIBRARY_MANIFEST.format(
                min_sdk=min_sdk_version, target_sdk=target_sdk_version
            )
        )


def _RetrieveBackportedMethods(backported_methods_path):
    with open(backported_methods_path, encoding='utf-8') as f:
        methods = f.read().splitlines()
    # Methods look like:
    #   java/util/Set#of(Ljava/lang/Object;)Ljava/util/Set;
    # But error message looks like:
    #   Call requires API level R (current min is 21): java.util.Set#of [NewApi]
    methods = (m.replace('/', '\\.') for m in methods)
    methods = (m[: m.index('(')] for m in methods)
    return sorted(set(methods))


def _SourceRootRegexp(pathvar_src):
    """Returns a regexp matching the source root as lint spells it in paths.

    Lint matches <ignore regexp="..."> against the absolute path of each file,
    which is <output dir>/../../path/in/src for sources, but the canonical
    /path/to/src/path/in/src for resource files (lint canonicalizes those).
    """
    return '(?:{}|{})'.format(
        re.escape(os.path.realpath(pathvar_src)), re.escape(pathvar_src)
    )


def _GenerateConfigXmlTree(orig_config_path, backported_methods, src_regexp):
    if orig_config_path:
        root_node = ElementTree.parse(orig_config_path).getroot()
        for ignore_node in root_node.iter('ignore'):
            regexp = ignore_node.get('regexp')
            if regexp and _SRC_PLACEHOLDER in regexp:
                ignore_node.set(
                    'regexp', regexp.replace(_SRC_PLACEHOLDER, src_regexp)
                )
    else:
        root_node = ElementTree.fromstring('<lint/>')

    issue_node = ElementTree.SubElement(root_node, 'issue')
    issue_node.attrib['id'] = 'NewApi'
    ignore_node = ElementTree.SubElement(issue_node, 'ignore')
    ignore_node.attrib['regexp'] = '|'.join(backported_methods)
    return root_node


def _WriteXmlFile(root, path):
    logging.info('Writing xml file %s', path)
    build_utils.MakeDirectory(os.path.dirname(path))
    with action_helpers.atomic_output(path, encoding='utf-8') as f:
        # Although we can write it just with ElementTree.tostring, using minidom
        # makes it a lot easier to read as a human (also on code search).
        f.write(
            minidom.parseString(
                ElementTree.tostring(root, encoding='utf-8')
            ).toprettyxml(indent='  ')
        )


def _PathVarSrc():
    """Returns the source root as <output dir>/../.., the SRC path variable."""
    root_path = os.getcwd()  # This is usually the output directory.
    return os.path.join(
        root_path, os.path.relpath(build_utils.DIR_SOURCE_ROOT, start=root_path)
    )


def _LintCmd(
    lint_jar_path,
    android_sdk_root,
    xmx,
    disabled,
    jvm_flags=None,
    native_lint_path=None,
):
    # All paths in lint are based off of relative paths from root with root as the
    # prefix. Path variable substitution is based off of prefix matching so custom
    # path variables need to match exactly in order to show up in baseline files.
    # e.g. lint_path=path/to/output/dir/../../file/in/src
    root_path = os.getcwd()  # This is usually the output directory.
    pathvar_src = _PathVarSrc()
    if native_lint_path and _IS_LINUX_X64:
        # A native image takes -Xmx but rejects most JVM flags. Size G1's
        # threads for 2 CPUs, since many lint processes run in parallel.
        cmd = [native_lint_path, '-Xmx' + xmx, '-XX:ActiveProcessorCount=2']
    else:
        cmd = build_utils.JavaCmd(xmx=xmx) + (jvm_flags or [])
        cmd += ['-cp', lint_jar_path, 'com.android.tools.lint.Main']
    return cmd + [
        '--sdk-home',
        android_sdk_root,
        '--jdk-home',
        build_utils.JAVA_HOME,
        '--path-variables',
        # Partial results store paths with these variables. Without OUT,
        # generated files come out of report steps as ../../out/<dir>/gen/...
        # rather than gen/..., since SRC is a prefix of the output directory.
        f'OUT={root_path};SRC={pathvar_src}',
        '--offline',
        '--quiet',  # Silences lint's "." progress updates.
        '--stacktrace',  # Prints full stacktraces for internal lint errors.
        # Some custom lint checks are not always available, which can cause
        # UnknownIssueId warnings. Disable them to avoid noise.
        '--disable',
        ','.join(disabled),
    ]


def _StdoutFilter(output):
    filter_patterns = [
        # This filter is necessary for JDK11.
        'No issues found',
        # Custom checks are not always available in every lint run so an
        # UnknownIssueId warning is sometimes printed for custom checks in the
        # _DISABLED_ALWAYS list.
        r'\[UnknownIssueId\]',
        # If all the warnings are filtered, we should not fail on the final
        # summary line.
        r'\d+ errors?, \d+ warnings?',
    ]
    return build_utils.FilterLines(output, '|'.join(filter_patterns))


def _Run(cmd, lint_gen_dir, allow_created_baseline, fail_on_output):
    """Runs lint. Returns False if it failed (and its output was printed)."""
    if allow_created_baseline:
        # Allow error code 6 when creating a baseline: ERRNO_CREATED_BASELINE
        fail_func = lambda returncode, _: returncode not in (0, 6)  # noqa: E731
    else:
        fail_func = lambda returncode, _: returncode != 0  # noqa: E731
    # Lint writes an analytics.settings file here.
    env = os.environ.copy()
    env['ANDROID_SDK_HOME'] = lint_gen_dir
    start = time.time()
    try:
        build_utils.CheckOutput(
            cmd,
            env=env,
            print_stdout=True,
            stdout_filter=_StdoutFilter,
            stderr_filter=build_utils.FilterReflectiveAccessJavaWarnings,
            fail_on_output=fail_on_output,
            fail_func=fail_func,
        )
        return True
    except build_utils.CalledProcessError as e:
        # Do not output the python stacktrace because it is lengthy and is not
        # relevant to the actual lint error.
        sys.stderr.write(e.output)
        return False
    finally:
        logging.info('Lint command took %ss', time.time() - start)


def _PrintFailureHelp(is_debug, project_xml_path):
    print('- For more help with lint in Chrome:', _LINT_MD_URL)
    if is_debug:
        print(
            '- DEBUG MODE: Here is the project.xml: {}'.format(
                _SrcRelative(project_xml_path)
            )
        )
    else:
        print('- Run with LINT_DEBUG=1 to enable lint configuration debugging')


def _WriteConfig(lint_gen_dir, config_path, backported_methods_path):
    logging.info('Generating config.xml')
    backported_methods = _RetrieveBackportedMethods(backported_methods_path)
    config_xml_node = _GenerateConfigXmlTree(
        config_path, backported_methods, _SourceRootRegexp(_PathVarSrc())
    )
    generated_config_path = os.path.join(lint_gen_dir, 'config.xml')
    _WriteXmlFile(config_xml_node, generated_config_path)
    return generated_config_path


def _RunReport(
    lint_jar_path,
    backported_methods_path,
    config_path,
    android_sdk_root,
    lint_gen_dir,
    baseline,
    app_lint_gen_dir,
    dep_partials_dirs,
    warnings_as_errors=False,
    native_lint_path=None,
):
    """Merges partial results of the app and its libraries and reports."""
    creating_baseline = baseline and not os.path.exists(baseline)
    build_utils.MakeDirectory(lint_gen_dir)
    project = ElementTree.parse(
        os.path.join(app_lint_gen_dir, 'project.xml')
    ).getroot()
    main_module = project.find('module')
    for dep_partials_dir in dep_partials_dirs:
        _AddDepModule(
            project,
            main_module,
            _ModuleName(dep_partials_dir),
            partials_dir=dep_partials_dir,
        )
    if baseline:
        ElementTree.SubElement(project, 'baseline').set('file', baseline)
    project_xml_path = os.path.join(lint_gen_dir, 'project.xml')
    _WriteXmlFile(project, project_xml_path)

    cmd = _LintCmd(
        lint_jar_path,
        android_sdk_root,
        '6G' if creating_baseline else '4G',
        _DISABLED_ALWAYS,
        native_lint_path=native_lint_path,
    )
    cmd += [
        '--config',
        _WriteConfig(lint_gen_dir, config_path, backported_methods_path),
        '--project',
        project_xml_path,
        '--report-only',
    ]
    if not _Run(
        cmd,
        lint_gen_dir,
        creating_baseline and not warnings_as_errors,
        warnings_as_errors,
    ):
        _PrintFailureHelp(
            os.environ.get('LINT_DEBUG', '0') != '0', project_xml_path
        )
        sys.exit(1)


def _RunLint(
    lint_jar_path,
    backported_methods_path,
    config_path,
    sources,
    classpath,
    cache_dir,
    android_sdk_version,
    aars,
    srcjars,
    resource_sources,
    resource_zips,
    android_sdk_root,
    lint_gen_dir,
    baseline,
    manifest_path,
    warnings_as_errors=False,
    aar_dirs=None,
    mode='full',
    annotation_zips=None,
    min_sdk_version=None,
    target_sdk_version=None,
    jvm_flags=None,
    native_lint_path=None,
):
    """Runs lint over one module.

    Modes:
      full: Analyzes and reports on the whole app in one lint invocation.
      analyze-library: Writes partial results for one library module, without
          any knowledge of the apks that include it (lint --analyze-only).
      analyze-app: Writes partial results for the app module: resources, and
          sources that belong to no analyzed library.

    """
    logging.info('Lint starting')
    analyze = mode != 'full'
    is_per_target_cache = not cache_dir
    if is_per_target_cache:
        # Use per-target cache directory when --cache-dir is not used.
        cache_dir = os.path.join(lint_gen_dir, 'android_lint_cache')
        # Clear any stale or partially extracted AARs from interrupted runs;
        # Lint skips unpacking an AAR if its cache subdirectory already exists.
        shutil.rmtree(cache_dir, ignore_errors=True)
        # Lint complains if the directory does not exist.
        os.makedirs(cache_dir)

    if baseline and not os.path.exists(baseline) and not analyze:
        # Generating new baselines is only done locally, and requires more memory to
        # avoid OOMs.
        creating_baseline = True
        lint_xmx = '6G'
    else:
        creating_baseline = False
        lint_xmx = '4G'
    disabled = _DISABLED_ALWAYS
    if mode == 'analyze-library':
        # One library needs far less than the whole app.
        lint_xmx = '2G'
        disabled = disabled + _DISABLED_IN_LIBRARIES

    # Lint requires this directory to exist and be cleared. See b/324598620.
    partials_dir = os.path.join(lint_gen_dir, 'partials')
    shutil.rmtree(partials_dir, ignore_errors=True)
    os.makedirs(partials_dir)

    cmd = _LintCmd(
        lint_jar_path,
        android_sdk_root,
        lint_xmx,
        disabled,
        jvm_flags,
        native_lint_path,
    )
    cmd += [
        '--config',
        _WriteConfig(lint_gen_dir, config_path, backported_methods_path),
    ]

    resource_root_dir = os.path.join(lint_gen_dir, _RES_ZIP_DIR)
    shutil.rmtree(resource_root_dir, True)
    # These are zip files with generated resources (e. g. strings from GRD).
    logging.info('Extracting resource zips')
    for resource_zip in resource_zips:
        # Use a consistent root and name rather than a temporary file so that
        # suppressions can be local to the lint target and the resource target.
        resource_dir = os.path.join(resource_root_dir, resource_zip)
        os.makedirs(resource_dir)
        resource_sources.extend(
            build_utils.ExtractAll(resource_zip, path=resource_dir)
        )

    logging.info('Extracting srcjars')
    srcjar_root_dir = os.path.join(lint_gen_dir, _SRCJAR_DIR)
    shutil.rmtree(srcjar_root_dir, True)
    srcjar_sources = []
    if srcjars:
        for srcjar in srcjars:
            # Use path without extensions since otherwise the file name includes
            # .srcjar and lint treats it as a srcjar.
            srcjar_dir = os.path.join(
                srcjar_root_dir, os.path.splitext(srcjar)[0]
            )
            os.makedirs(srcjar_dir)
            # Sadly lint's srcjar support is broken since it only considers the first
            # srcjar. Until we roll a lint version with that fixed, we need to extract
            # it ourselves.
            srcjar_sources.extend(
                build_utils.ExtractAll(srcjar, path=srcjar_dir)
            )

    if mode == 'analyze-library':
        manifest_path = os.path.join(lint_gen_dir, 'AndroidManifest.xml')
        _WriteLibraryManifest(
            manifest_path, min_sdk_version, target_sdk_version
        )
    logging.info('Generating project file')
    project_file_root = _GenerateProjectFile(
        manifest_path,
        android_sdk_root,
        cache_dir,
        partials_dir,
        sources,
        classpath,
        srcjar_sources,
        resource_sources,
        aars,
        android_sdk_version,
        None if analyze else baseline,
        aar_dirs=aar_dirs,
        library=mode == 'analyze-library',
        annotation_zips=annotation_zips,
    )

    project_xml_path = os.path.join(lint_gen_dir, 'project.xml')
    _WriteXmlFile(project_file_root, project_xml_path)
    cmd += ['--project', project_xml_path]
    if analyze:
        # Partial analysis reports nothing: issues come out of --report-only.
        cmd += ['--analyze-only']

    ok = _Run(
        cmd,
        lint_gen_dir,
        creating_baseline and not warnings_as_errors,
        warnings_as_errors and not analyze,
    )

    # When not treating warnings as errors, display the extra footer.
    is_debug = os.environ.get('LINT_DEBUG', '0') != '0'
    # Analysis modes keep their partial results, project.xml and the extracted
    # srcjars and resources that it points to, since the report step reads them.
    if not is_debug and not analyze:
        shutil.rmtree(resource_root_dir, ignore_errors=True)
        shutil.rmtree(srcjar_root_dir, ignore_errors=True)
        if os.path.exists(project_xml_path):
            os.unlink(project_xml_path)
        shutil.rmtree(partials_dir, ignore_errors=True)
    # The report step reuses the app's project.xml, which names its cache dir.
    if not is_debug and is_per_target_cache and mode != 'analyze-app':
        shutil.rmtree(cache_dir, ignore_errors=True)

    if not ok:
        _PrintFailureHelp(is_debug, project_xml_path)
        sys.exit(1)

    logging.info('Lint completed')


_PACKAGE_RE = re.compile(r'^package\s+([\w.]+)', flags=re.MULTILINE)


def _SourceRoot(path):
    """Returns the directory that |path|'s package directories are in."""
    with open(path, encoding='utf-8') as f:
        m = _PACKAGE_RE.search(f.read())
    root = os.path.dirname(path)
    if m:
        package_dir = m.group(1).replace('.', os.sep)
        if root.endswith(os.sep + package_dir):
            root = root[: -len(package_dir) - 1]
    return root


def _ExtractAnnotations(
    lint_jar_path,
    native_lint_path,
    sources,
    srcjars,
    classpath,
    lint_gen_dir,
    output,
):
    """Writes the typedefs and other annotations of a library to |output|.

    Uses lint-gradle's annotation extractor, which writes the external
    annotations zip that lint reads for libraries. Writes an empty zip when the
    library has none, and leaves |output| alone when it would not change, so
    that only API changes re-analyze dependents.
    """
    os.makedirs(lint_gen_dir, exist_ok=True)
    srcjar_root_dir = os.path.join(lint_gen_dir, _SRCJAR_DIR)
    shutil.rmtree(srcjar_root_dir, True)
    # The extractor resolves annotations through --source-roots, and derives
    # them from the sources when not given. Derived roots can be ancestors of
    # the real ones (e.g. mojo/public/java for mojo/public/java/system/src),
    # whose .java files it all reads, including those of other targets that
    # may be deleted mid-read.
    source_roots = {_SourceRoot(p) for p in sources}
    sources = list(sources)
    for srcjar in srcjars:
        srcjar_dir = os.path.join(srcjar_root_dir, os.path.splitext(srcjar)[0])
        sources += build_utils.ExtractAll(srcjar, path=srcjar_dir)
        source_roots.add(srcjar_dir)
    # Paths are read from a file, since --sources is one argument.
    sources_path = os.path.join(lint_gen_dir, 'annotation_sources.txt')
    with open(sources_path, 'w', encoding='utf-8') as f:
        f.write(''.join(path + '\n' for path in sources))
    args = [
        '--sources',
        '@' + sources_path,
        '--source-roots',
        ':'.join(sorted(source_roots)),
        '--classpath',
        ':'.join(classpath),
        '--quiet',
    ]
    if native_lint_path and _IS_LINUX_X64:
        cmd = [native_lint_path, '-Xmx1G', '-XX:ActiveProcessorCount=2']
        cmd += ['--extract-annotations']
    else:
        cmd = build_utils.JavaCmd(xmx='1G') + _PER_LIBRARY_JVM_FLAGS
        cmd += [
            '-cp',
            lint_jar_path,
            'com.android.tools.lint.annotations.ExtractAnnotationsDriver',
        ]
    # The extractor resolves JDK classes from java.home, which the native
    # image takes from $JAVA_HOME.
    env = os.environ.copy()
    env['JAVA_HOME'] = build_utils.JAVA_HOME
    with action_helpers.atomic_output(output) as f:
        build_utils.CheckOutput(
            cmd + args + ['--output', f.name],
            env=env,
            print_stderr=False,
            fail_on_output=False,
        )
        if not os.path.exists(f.name):
            # Nothing to extract: the extractor deletes the output. An empty
            # file (common: most libraries have no typedefs) tells dependents
            # to skip it.
            open(f.name, 'w').close()
    if os.environ.get('LINT_DEBUG', '0') == '0':
        shutil.rmtree(srcjar_root_dir, ignore_errors=True)


def _ParseArgs(argv):
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)
    parser.add_argument('--target-name', help='Fully qualified GN target name.')
    parser.add_argument(
        '--mode',
        choices=[
            'full',
            'analyze-library',
            'analyze-app',
            'report',
            'extract-annotations',
        ],
        default='full',
        help='See _RunLint(). report: merges the partial results of '
        '--app-lint-gen-dir and --partials-dirs. extract-annotations: writes '
        'the typedefs of --sources and --srcjars to --output.',
    )
    parser.add_argument(
        '--min-sdk-version',
        help='analyze-library: minSdkVersion to give the library module.',
    )
    parser.add_argument(
        '--target-sdk-version',
        help='analyze-library: targetSdkVersion to give the library module.',
    )
    parser.add_argument(
        '--annotation-zips',
        help='analyze-*: GN list of extract-annotations outputs of '
        'dependencies, for their typedefs.',
    )
    parser.add_argument(
        '--output', help='extract-annotations: Path of the zip to write.'
    )
    parser.add_argument(
        '--app-lint-gen-dir',
        help='report: --lint-gen-dir of the analyze-app step.',
    )
    parser.add_argument(
        '--partials-dirs',
        help='report: GN list of partial results dirs of analyzed libraries.',
    )
    parser.add_argument(
        '--use-build-server',
        action='store_true',
        help='Always use the build server.',
    )
    parser.add_argument(
        '--lint-jar-path', required=True, help='Path to the lint jar.'
    )
    parser.add_argument(
        '--native-lint-path',
        help='Native image of the lint jar (lint-graalvm) to run instead of '
        'the JVM on linux-x64.',
    )
    parser.add_argument(
        '--backported-methods',
        help='Path to backported methods file created by R8.',
    )
    parser.add_argument(
        '--cache-dir',
        help='Path to the directory in which the android cache '
        'directory tree should be stored.',
    )
    parser.add_argument('--config-path', help='Path to lint suppressions file.')
    parser.add_argument(
        '--lint-gen-dir',
        required=True,
        help='Path to store generated xml files.',
    )
    parser.add_argument('--stamp', help='Path to stamp upon success.')
    parser.add_argument(
        '--android-sdk-version',
        help='Version (API level) of the Android SDK used for building.',
    )
    parser.add_argument(
        '--android-sdk-root',
        required=True,
        help='Lint needs an explicit path to the android sdk.',
    )
    parser.add_argument(
        '--warnings-as-errors',
        action='store_true',
        help='Treat all warnings as errors.',
    )
    parser.add_argument(
        '--sources',
        help='A list of files containing java and kotlin source files.',
    )
    parser.add_argument('--aars', help='GN list of included aars.')
    parser.add_argument(
        '--aar-dirs',
        help='GN list of directories that the --aars are unpacked into, in '
        'the same order as --aars.',
    )
    parser.add_argument('--srcjars', help='GN list of included srcjars.')
    parser.add_argument(
        '--manifest', help='Path to the merged AndroidManifest.xml.'
    )
    parser.add_argument(
        '--resource-sources',
        default=[],
        action='append',
        help='GYP-list of resource sources files, similar to '
        'java sources files, but for resource files.',
    )
    parser.add_argument(
        '--resource-zips',
        default=[],
        action='append',
        help='GYP-list of resource zips, zip files of generated '
        'resource files.',
    )
    parser.add_argument(
        '--classpath',
        action='append',
        help='GN list of jars to add to the classpath. Can be repeated.',
    )
    parser.add_argument(
        '--depfile-classpath',
        action='append',
        help='analyze-library: GN list of jars whose changes re-analyze the '
        'library (default: --classpath). Can be repeated.',
    )
    parser.add_argument(
        '--baseline',
        help='Baseline file to ignore existing errors and fail on new errors.',
    )

    args = parser.parse_args(build_utils.ExpandFileArgs(argv))
    args.sources = action_helpers.parse_gn_list(args.sources)
    args.aars = action_helpers.parse_gn_list(args.aars)
    args.aar_dirs = action_helpers.parse_gn_list(args.aar_dirs)
    if args.aar_dirs:
        assert len(args.aar_dirs) == len(args.aars), 'Need one dir per aar.'
    args.srcjars = action_helpers.parse_gn_list(args.srcjars)
    args.resource_sources = action_helpers.parse_gn_list(args.resource_sources)
    args.resource_zips = action_helpers.parse_gn_list(args.resource_zips)
    args.classpath = action_helpers.parse_gn_list(args.classpath)
    args.depfile_classpath = action_helpers.parse_gn_list(
        args.depfile_classpath
    )
    args.annotation_zips = action_helpers.parse_gn_list(args.annotation_zips)
    args.partials_dirs = action_helpers.parse_gn_list(args.partials_dirs)

    if args.baseline:
        assert os.path.basename(args.baseline) == 'lint-baseline.xml', (
            'The baseline file needs to be named "lint-baseline.xml" in order for '
            'the autoroller to find and update it whenever lint is rolled to a new '
            'version.'
        )

    return args


def main():
    build_utils.InitLogging('LINT_DEBUG')
    args = _ParseArgs(sys.argv[1:])

    if args.mode == 'report':
        _RunReport(
            args.lint_jar_path,
            args.backported_methods,
            args.config_path,
            args.android_sdk_root,
            args.lint_gen_dir,
            args.baseline,
            args.app_lint_gen_dir,
            args.partials_dirs,
            warnings_as_errors=args.warnings_as_errors,
            native_lint_path=args.native_lint_path,
        )
        # The baseline is a GN input only for bots' analyze (see
        # internal_rules.gni), so that deleting it locally regenerates it.
        if args.depfile:
            action_helpers.write_depfile(
                args.depfile,
                args.stamp,
                [args.baseline] if args.baseline else [],
            )
        build_utils.Touch(args.stamp)
        return

    sources = []
    for sources_file in args.sources:
        sources.extend(build_utils.ReadSourcesList(sources_file))

    # Header jars of deps are listed so that a dep's API change (e.g. a new
    # @RequiresApi or constant value) re-runs dependents.
    depfile_classpath = args.depfile_classpath or args.classpath

    if args.mode == 'extract-annotations':
        _ExtractAnnotations(
            args.lint_jar_path,
            args.native_lint_path,
            sources,
            args.srcjars,
            args.classpath,
            args.lint_gen_dir,
            args.output,
        )
        if args.depfile:
            action_helpers.write_depfile(
                args.depfile,
                args.output,
                sources + args.srcjars + depfile_classpath,
            )
        return
    resource_sources = []
    for resource_sources_file in args.resource_sources:
        resource_sources.extend(
            build_utils.ReadSourcesList(resource_sources_file)
        )

    possible_depfile_deps = (
        args.srcjars
        + args.resource_zips
        + sources
        + resource_sources
        + [args.baseline, args.manifest]
    )
    depfile_deps = [p for p in possible_depfile_deps if p]

    # _RunLint ignores the baseline and warnings_as_errors in partial modes.
    run_lint = functools.partial(
        _RunLint,
        args.lint_jar_path,
        args.backported_methods,
        args.config_path,
        sources,
        args.classpath,
        args.cache_dir,
        args.android_sdk_version,
        args.aars,
        args.srcjars,
        resource_sources,
        args.resource_zips,
        args.android_sdk_root,
        args.lint_gen_dir,
        args.baseline,
        args.manifest,
        warnings_as_errors=args.warnings_as_errors,
        aar_dirs=dict(zip(args.aars, args.aar_dirs)),
        native_lint_path=args.native_lint_path,
    )

    if args.mode != 'full':
        # Partial analysis does not use the build server.
        run_lint(
            mode=args.mode,
            annotation_zips=args.annotation_zips,
            min_sdk_version=args.min_sdk_version,
            target_sdk_version=args.target_sdk_version,
            jvm_flags=_PER_LIBRARY_JVM_FLAGS,
        )
        if args.depfile:
            action_helpers.write_depfile(
                args.depfile,
                args.stamp,
                depfile_deps + depfile_classpath + args.annotation_zips,
            )
        build_utils.Touch(args.stamp)
        return

    if args.depfile:
        action_helpers.write_depfile(args.depfile, args.stamp, depfile_deps)

    # Avoid parallelizing cache creation since lint runs without the cache defeat
    # the purpose of creating the cache in the first place. Forward the command
    # after the depfile has been written as siso requires it.
    if server_utils.MaybeRunCommand(
        name=args.target_name,
        argv=sys.argv,
        stamp_file=args.stamp,
        use_build_server=args.use_build_server,
    ):
        return

    run_lint()
    logging.info('Creating stamp file')
    server_utils.MaybeTouch(args.stamp)


if __name__ == '__main__':
    sys.exit(main())
