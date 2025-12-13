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
ERRORPRONE_CHECKS_TO_APPLY = [
    # Be sure to first update "android/errorprone" within
    # build/config/siso/android.star to set "remote": False.
]

# Checks to disable in tests.
TESTONLY_ERRORPRONE_WARNINGS_TO_DISABLE = [
    # Can hurt readability to enforce this on test classes.
    'FieldCanBeStatic',
    # These are allowed in tests.
    'NoStreams',
    # Too much effort to enable.
    'UnusedVariable',
]

# Full list of checks: https://errorprone.info/bugpatterns
ERRORPRONE_WARNINGS_TO_DISABLE = [
    # High priority to enable in non-tests:
    'StaticAssignmentInConstructor',
    # CheckReturnValue: See note below about enabling via -Xep.

    # Still to look into:
    'AnnotationPosition',
    'AvoidObjectArrays',
    'BanSerializableRead',
    'BooleanParameter',
    'CannotMockMethod',
    'CatchingUnchecked',
    'CheckedExceptionNotThrown',
    'ConstantField',
    'ConstantPatternCompile',
    'DeduplicateConstants',
    'DefaultLocale',
    'DepAnn',
    'DifferentNameButSame',
    'ExpectedExceptionChecker',
    'ForEachIterable',
    'FunctionalInterfaceClash',
    'IdentifierName',
    'ImmutableMemberCollection',
    'InconsistentOverloads',
    'InitializeInline',
    'InterruptedExceptionSwallowed',
    'Interruption',
    'MethodCanBeStatic',
    'MissingDefault',
    'MixedArrayDimensions',
    'MockitoDoSetup',
    'NegativeBoolean',
    'NonCanonicalStaticMemberImport',
    'NonFinalStaticField',
    'PreferJavaTimeOverload',
    'PreferredInterfaceType',
    'PrimitiveArrayPassedToVarargsMethod',
    'PrivateConstructorForUtilityClass',
    'RedundantThrows',
    'ReturnsNullCollection',
    'StringFormatWithLiteral',
    'SuppressWarningsWithoutExplanation',
    'SystemExitOutsideMain',
    'SystemOut',
    'TestExceptionChecker',
    'ThrowSpecificExceptions',
    'ThrowsUncheckedException',
    'TooManyParameters',
    'TryFailRefactoring',
    'TypeParameterNaming',
    'UngroupedOverloads',
    'UnnecessaryAnonymousClass',
    'UnnecessaryBoxedAssignment',
    'UnnecessaryDefaultInEnumSwitch',
    'UnnecessaryFinal',
    'UnsafeLocaleUsage',
    'UnusedException',
    'UseEnumSwitch',
    'UsingJsr305CheckReturnValue',
    'Var',
    'Varifier',
    'YodaCondition',

    # Low priority.
    'CatchAndPrintStackTrace',
    'EqualsHashCode',
    'JavaUtilDate',
    'OverrideThrowableToString',
    'ParameterComment',
    'PatternMatchingInstanceof',
    'StatementSwitchToExpressionSwitch',
    'UndefinedEquals',
    'StaticAssignmentOfThrowable',  # Want in non-test
    'StaticMockMember',
    'StringCaseLocaleUsage',
    'StringCharset',
    'ThreadLocalUsage',
    'TypeParameterUnusedInFormals',
    'UnnecessaryBoxedVariable',
    'UnnecessarilyFullyQualified',
    'UnsafeReflectiveConstructionCast',

    # Never Enable:
    #
    # Chromium uses assert statements.
    'AssertFalse',
    # Debatable whether it makes code less readable by forcing larger names for
    # "Builder".
    'BadImport',
    # Such modifiers in nested classes do not hurt readability IMO.
    'EffectivelyPrivate',
    # Android APIs sometimes throw random exceptions that are safe to ignore.
    'EmptyCatch',
    # Just use Android Studio refactors to inline things.
    'InlineMeInliner',
    'InlineMeSuggester',
    # We already have presubmit checks for this. We don't want it to fail
    # local compiles.
    'RemoveUnusedImports',
    # Several instances of using a string right before the String.format(),
    # which seems better than inlining.
    'InlineFormatString',
    # Assigning to fields marked as @Mock or @Spy. Suggested fix is to delete
    # assignments, which would break tests in many cases.
    'UnnecessaryAssignment',
    # Android platform default is always UTF-8.
    # https://developer.android.com/reference/java/nio/charset/Charset.html#defaultCharset()
    'DefaultCharset',
    # If google-java-format is not going to do this, it's not worth our time.
    'StringConcatToTextBlock',
    # We don't use Dagger.
    'RefersToDaggerCodegen',
    # Only has false positives (would not want to enable this).
    'UnicodeEscape',
    # Does not apply to Android because it assumes no desugaring.
    'UnnecessaryLambda',
    # These are best practices that I doubt are worth the churn / overhead.
    'MixedMutabilityReturnType',
    'MutablePublicArray',
    'NonApiType',
    # Not that useful.
    'ClassNewInstance',
    # Low priority corner cases with String.split.
    # Linking Guava and using Splitter was rejected
    # in the https://chromium-review.googlesource.com/c/chromium/src/+/871630.
    'StringSplitter',
    # There are lots of times when we just want to post a task.
    'FutureReturnValueIgnored',
    # Just false positives in our code.
    'ThreadJoinLoop',
    # Fine to run the auto-fix from time to time (which replaces assert
    # statements with Truth assertions), but because using assert statements is
    # normal in non-test code, they also show up in test helpers, which are
    # arguably false-positives.
    'UseCorrectAssertInTests',
    # NullAway makes these redundant.
    'FieldMissingNullable',
    'ParameterMissingNullable',
    'ReturnMissingNullable',
    # Style guide difference between google3 & chromium.
    'MissingBraces',
    # Does not seem to take into account R8 backports. Redundant with Android
    # Lint anyways.
    'Java8ApiChecker',
    # Style guide difference between google3 & chromium.
    'UnnecessaryTestMethodPrefix',
    # Too many suggestions where it's not actually necessary.
    'CanIgnoreReturnValueSuggester',

    # These are all for Javadoc, which we don't really care about.
    'InvalidBlockTag',
    'InvalidInlineTag',
    'InvalidLink',
    'InvalidParam',
    'MalformedInlineTag',
    'MissingSummary',
    'NotJavadoc',
    'UnescapedEntity',
    'UnrecognisedJavadocTag',
]

# Full list of checks: https://errorprone.info/bugpatterns
# Only those marked as "experimental" need to be listed here in order to be
# enabled.
ERRORPRONE_WARNINGS_TO_ENABLE = [
    'BinderIdentityRestoredDangerously',
    'EmptyIf',
    'EqualsBrokenForNull',
    'FieldCanBeFinal',
    'FieldCanBeLocal',
    'FieldCanBeStatic',
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
  if server_utils.MaybeRunCommand(
      name=options.stamp,
      argv=sys.argv,
      stamp_file=options.stamp,
      use_build_server=options.use_build_server):
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

    # TODO(agrieve): Re-enable once we sort out nullability of
    #     ObservableSuppliers. https://crbug.com/430320400
    # Make it a warning to use assumeNonNull() with a @NonNull.
    #errorprone_flags += [('-XepOpt:NullAway:CastToNonNullMethod='
    #                      'org.chromium.build.NullUtil.assumeNonNull')]
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
    # These are in addition to the default list in "DEFAULT_KNOWN_INITIALIZERS":
    # https://github.com/uber/NullAway/blob/d5cb4f1190a96045d85b92c6d119e4595840cc8a/nullaway/src/main/java/com/uber/nullaway/ErrorProneCLIFlagsConfig.java#L128
    init_methods = [
        'android.app.backup.BackupAgent.onCreate',
        'android.content.ContentProvider.attachInfo',
        'android.content.ContentProvider.onCreate',
        'android.content.ContextWrapper.attachBaseContext',
        'androidx.preference.PreferenceFragmentCompat.onCreatePreferences',
    ]
    errorprone_flags += [
        '-XepOpt:NullAway:KnownInitializers=' + ','.join(init_methods)
    ]
    # Exclude fields with these annotations from null-checking.
    mock_annotations = [
        'org.mockito.Captor',
        'org.mockito.Mock',
        'org.mockito.Spy',
    ]
    errorprone_flags += [
        '-XepOpt:NullAway:ExcludedFieldAnnotations=' +
        ','.join(mock_annotations)
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
    errorprone_flags += ['-XepCompilingTestOnlyCode']

  # To enable CheckReturnValue to be opt-out rather than opt-in:
  # packages = 'org.chromium,com.google,java.util.regex'
  # errorprone_flags += ['-XepOpt:CheckReturnValue:Packages=' + packages]
  # errorprone_flags += ['-XepOpt:CheckReturnValue:CheckAllConstructors=true']
  # Might also need "-XepOpt:CheckReturnValue:ApiExclusionList=" with a
  # "cirvlist.txt" that annotates android.* / java.* as @CanIgnoreReturnValue.

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
