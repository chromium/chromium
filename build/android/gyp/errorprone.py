#!/usr/bin/env python3
#
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run Error Prone."""

import argparse
import sys

import compile_java
from util import server_utils

# Add a check here to cause the suggested fix to be applied while compiling.
# Use this when trying to enable more checks.
ERRORPRONE_CHECKS_TO_APPLY = []

# Checks to disable in tests.
TESTONLY_ERRORPRONE_WARNINGS_TO_DISABLE = [
    # Too much effort to enable.
    'UnusedVariable',
    # These are allowed in tests.
    'NoStreams',
]

# Full list of checks: https://errorprone.info/bugpatterns
ERRORPRONE_WARNINGS_TO_DISABLE = [
    'InlineMeInliner',
    'InlineMeSuggester',
    # High priority to enable:
    'HidingField',
    'AlreadyChecked',
    'DirectInvocationOnMock',
    'MockNotUsedInProduction',
    'PatternMatchingInstanceof',
    'AssignmentExpression',
    'RuleNotRun',
    # High priority to enable in non-tests:
    'JdkObsolete',
    'ReturnValueIgnored',
    'StaticAssignmentInConstructor',
    # These are all for Javadoc, which we don't really care about.
    # vvv
    'InvalidBlockTag',
    'InvalidParam',
    'InvalidLink',
    'InvalidInlineTag',
    'MalformedInlineTag',
    'MissingSummary',
    'UnescapedEntity',
    'UnrecognisedJavadocTag',
    # ^^^
    'MutablePublicArray',
    'NonCanonicalType',
    'DoNotClaimAnnotations',
    'JavaUtilDate',
    'IdentityHashMapUsage',
    'StaticMockMember',
    # Triggers in tests where this is useful to do.
    'StaticAssignmentOfThrowable',
    # TODO(crbug.com/41384349): Follow steps in bug.
    'CatchAndPrintStackTrace',
    # TODO(crbug.com/41364806): Follow steps in bug.
    'TypeParameterUnusedInFormals',
    # Android platform default is always UTF-8.
    # https://developer.android.com/reference/java/nio/charset/Charset.html#defaultCharset()
    'DefaultCharset',
    # There are lots of times when we just want to post a task.
    'FutureReturnValueIgnored',
    # Just false positives in our code.
    'ThreadJoinLoop',
    # Low priority corner cases with String.split.
    # Linking Guava and using Splitter was rejected
    # in the https://chromium-review.googlesource.com/c/chromium/src/+/871630.
    'StringSplitter',
    # Preferred to use another method since it propagates exceptions better.
    'ClassNewInstance',
    # Results in false positives.
    'ThreadLocalUsage',
    # Low priority.
    'EqualsHashCode',
    # Not necessary for tests.
    'OverrideThrowableToString',
    # Not that useful.
    'UnsafeReflectiveConstructionCast',
    # Not that useful.
    'MixedMutabilityReturnType',
    # Nice to have.
    'EqualsGetClass',
    # A lot of false-positives from CharSequence.equals().
    'UndefinedEquals',
    # Dagger generated code triggers this.
    'SameNameButDifferent',
    # Does not apply to Android because it assumes no desugaring.
    'UnnecessaryLambda',
    # Nice to have.
    'EmptyCatch',
    # Nice to have.
    'BadImport',
    # Nice to have.
    'UseCorrectAssertInTests',
    # Must be off since we are now passing in annotation processor generated
    # code as a source jar (deduplicating work with turbine).
    'RefersToDaggerCodegen',
    # We already have presubmit checks for this. We don't want it to fail
    # local compiles.
    'RemoveUnusedImports',
    # Only has false positives (would not want to enable this).
    'UnicodeEscape',
    # A lot of existing violations. e.g. Should return List and not ArrayList
    'NonApiType',
    # Nice to have.
    'StringCharset',
    # Nice to have.
    'StringConcatToTextBlock',
    # Nice to have.
    'StringCaseLocaleUsage',
    # Low priority.
    'RedundantControlFlow',
    # Low priority.
    'StatementSwitchToExpressionSwitch',
]

# Full list of checks: https://errorprone.info/bugpatterns
# Only those marked as "experimental" need to be listed here in order to be
# enabled.
ERRORPRONE_WARNINGS_TO_ENABLE = [
    'BinderIdentityRestoredDangerously',
    'EmptyIf',
    'EqualsBrokenForNull',
    'InvalidThrows',
    'LongLiteralLowerCaseSuffix',
    'MultiVariableDeclaration',
    'RedundantOverride',
    'StaticQualifiedUsingExpression',
    'TimeUnitMismatch',
    'UnnecessaryStaticImport',
    'UseBinds',
    'WildcardImport',
    'NoStreams',
]


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--skip-build-server',
                      action='store_true',
                      help='Avoid using the build server.')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--testonly',
                      action='store_true',
                      help='Disable some Error Prone checks')
  parser.add_argument('--enable-nullaway',
                      action='store_true',
                      help='Enable NullAway (requires --enable-errorprone)')
  parser.add_argument('--stamp',
                      required=True,
                      help='Path of output .stamp file')
  options, compile_java_argv = parser.parse_known_args()

  compile_java_argv += ['--jar-path', options.stamp]

  # Use the build server for errorprone runs.
  if not options.skip_build_server and (server_utils.MaybeRunCommand(
      name=options.stamp,
      argv=sys.argv,
      stamp_file=options.stamp,
      use_build_server=options.use_build_server)):
    compile_java.main(compile_java_argv, write_depfile_only=True)
    return

  # All errorprone args are passed space-separated in a single arg.
  errorprone_flags = ['-Xplugin:ErrorProne']

  if options.enable_nullaway:
    # See: https://github.com/uber/NullAway/wiki/Configuration
    # Check nullability only for classes marked with @NullMarked (this is our
    # migration story).
    errorprone_flags += ['-XepOpt:NullAway:OnlyNullMarked']
    errorprone_flags += [
        '-XepOpt:NullAway:CustomContractAnnotations='
        'org.chromium.build.annotations.Contract,'
        'org.chromium.support_lib_boundary.util.Contract'
    ]
    # TODO(agrieve): Re-enable once this is fixed:
    #     https://github.com/uber/NullAway/issues/1104
    # errorprone_flags += ['-XepOpt:NullAway:CheckContracts=true']

    # Make it a warning to use assumeNonNull() with a @NonNull.
    errorprone_flags += [('-XepOpt:NullAway:CastToNonNullMethod='
                          'org.chromium.build.NullUtil.assumeNonNull')]
    # Detect "assert foo != null" as a null check.
    errorprone_flags += ['-XepOpt:NullAway:AssertsEnabled=true']
    # Do not ignore @Nullable & @NonNull in non-@NullMarked classes.
    errorprone_flags += [
        '-XepOpt:NullAway:AcknowledgeRestrictiveAnnotations=true'
    ]
    # Treat @RecentlyNullable the same as @Nullable.
    errorprone_flags += ['-XepOpt:Nullaway:AcknowledgeAndroidRecent=true']
    # Enable experimental checking of @Nullable generics.
    # https://github.com/uber/NullAway/wiki/JSpecify-Support
    errorprone_flags += ['-XepOpt:NullAway:JSpecifyMode=true']
    # Treat these the same as constructors.
    init_methods = [
        'android.app.Application.onCreate',
        'android.app.Activity.onCreate',
        'android.app.Service.onCreate',
        'android.app.backup.BackupAgent.onCreate',
        'android.content.ContentProvider.attachInfo',
        'android.content.ContentProvider.onCreate',
        'android.content.ContextWrapper.attachBaseContext',
        'androidx.preference.PreferenceFragmentCompat.onCreatePreferences',
    ]
    errorprone_flags += [
        '-XepOpt:NullAway:KnownInitializers=' + ','.join(init_methods)
    ]

  # Make everything a warning so that when treat_warnings_as_errors is false,
  # they do not fail the build.
  errorprone_flags += ['-XepAllErrorsAsWarnings']
  # Don't check generated files (those tagged with @Generated).
  errorprone_flags += ['-XepDisableWarningsInGeneratedCode']
  errorprone_flags.extend('-Xep:{}:OFF'.format(x)
                          for x in ERRORPRONE_WARNINGS_TO_DISABLE)
  errorprone_flags.extend('-Xep:{}:WARN'.format(x)
                          for x in ERRORPRONE_WARNINGS_TO_ENABLE)
  if options.testonly:
    errorprone_flags.extend('-Xep:{}:OFF'.format(x)
                            for x in TESTONLY_ERRORPRONE_WARNINGS_TO_DISABLE)

  if ERRORPRONE_CHECKS_TO_APPLY:
    to_apply = list(ERRORPRONE_CHECKS_TO_APPLY)
    if options.testonly:
      to_apply = [
          x for x in to_apply
          if x not in TESTONLY_ERRORPRONE_WARNINGS_TO_DISABLE
      ]
    errorprone_flags += [
        '-XepPatchLocation:IN_PLACE', '-XepPatchChecks:,' + ','.join(to_apply)
    ]

  # These are required to use JDK 16, and are taken directly from
  # https://errorprone.info/docs/installation
  javac_args = [
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.model=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.processing='
      'ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
      '-J--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
      '-J--add-opens=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
      '-J--add-opens=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED',
  ]

  javac_args += ['-XDcompilePolicy=simple', ' '.join(errorprone_flags)]

  javac_args += ['-XDshould-stop.ifError=FLOW']
  # This flag quits errorprone after checks and before code generation, since
  # we do not need errorprone outputs, this speeds up errorprone by 4 seconds
  # for chrome_java.
  if not ERRORPRONE_CHECKS_TO_APPLY:
    javac_args += ['-XDshould-stop.ifNoError=FLOW']

  compile_java.main(compile_java_argv,
                    extra_javac_args=javac_args,
                    use_errorprone=True)


if __name__ == '__main__':
  main()
