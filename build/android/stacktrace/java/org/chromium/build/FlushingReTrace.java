// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.tools.r8.DiagnosticsHandler;
import com.android.tools.r8.retrace.ProguardMappingSupplier;
import com.android.tools.r8.retrace.Retrace;
import com.android.tools.r8.retrace.RetraceCommand;
import com.android.tools.r8.retrace.StackTraceSupplier;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.List;

/**
 * A wrapper around ReTrace that:
 *  1. Hardcodes a more useful line regular expression
 *  2. Disables output buffering
 */
public class FlushingReTrace {
    // E.g.: D/ConnectivityService(18029): Message
    // E.g.: W/GCM     ( 151): Message
    // E.g.: 09-08 14:22:59.995 18029 18055 I ProcessStatsService: Message
    // E.g.: 09-08 14:30:59.145 17731 18020 D MDnsDS  : Message
    private static final String LOGCAT_PREFIX =
            "(?:[VDIWEF]/.*?\\( *\\d+\\): |\\d\\d-\\d\\d [0-9:. ]+[VDIWEF] .*?: )?";

    // Note: Order of these sub-patterns defines their precedence.
    // Note: Deobfuscation of methods without the presense of line numbers basically never works.
    // There is a test for these pattern at //build/android/stacktrace/java_deobfuscate_test.py
    private static final String LINE_PARSE_REGEX =
            // Eagerly match logcat prefix to avoid conflicting with the patterns below.
            LOGCAT_PREFIX
                    + "(?:"
                    // Based on default ReTrace regex, but with whitespaces allowed in file:line
                    // parentheses and "at" changed to to allow :
                    // E.g.: 06-22 13:58:02.895  4674  4674 E THREAD_STATE:     bLA.a( PG : 173 )
                    // Normal stack trace lines look like:
                    // \tat org.chromium.chrome.browser.tab.Tab.handleJavaCrash(Tab.java:682)
                    + "(?:.*?(?::|\\bat)\\s+%c\\.%m\\s*\\(\\s*%s(?:\\s*:\\s*%l\\s*)?\\))|"
                    // Stack trace from crbug.com/1300215 looks like:
                    // 0xffffffff (chromium-TrichromeChromeGoogle.aab-canary-490400033: 70) ii2.p
                    + "(?:.*?\\(\\s*%s(?:\\s*:\\s*%l\\s*)?\\)\\s*%c\\.%m)|"
                    // E.g.: Caused by: java.lang.NullPointerException: Attempt to read from field
                    // 'int bLA' on a null object reference
                    + "(?:.*java\\.lang\\.NullPointerException.*[\"']%t\\s*%c\\.(?:%f|%m\\(%a\\))[\"'].*)|"
                    // E.g.: java.lang.VerifyError: bLA
                    + "(?:java\\.lang\\.VerifyError: %c)|"
                    // E.g.: java.lang.NoSuchFieldError: No instance field e of type L...; in class
                    // LbxK;
                    + "(?:java\\.lang\\.NoSuchFieldError: No instance field %f of type .*? in class"
                    + " L%C;)|"
                    // E.g.: Object of type Clazz was not destroyed... (See LifetimeAssert.java)
                    + "(?:.*?Object of type %c .*)|"
                    // E.g.: VFY: unable to resolve new-instance 3810 (LSome/Framework/Class;) in
                    // Lfoo/Bar;
                    + "(?:.*L%C;.*)|"
                    // E.g.: END SomeTestClass#someMethod
                    + "(?:.*?%c#%m.*?)|"
                    // Special-case for a common junit logcat message:
                    // E.g.: java.lang.NoClassDefFoundError: SomeFrameworkClass in isTestClass for
                    // Foo
                    + "(?:.* isTestClass for %c)|"
                    // E.g.: Caused by: java.lang.RuntimeException: Intentional Java Crash
                    + "(?:Caused by: %c:.*)|"
                    // Quoted values and lines that end with a class / class+method:
                    // E.g.: The class: Foo
                    // E.g.: INSTRUMENTATION_STATUS: class=Foo
                    // E.g.: NoClassDefFoundError: SomeFrameworkClass in isTestClass for Foo
                    // E.g.: Could not find class 'SomeFrameworkClass', referenced from method
                    // Foo.bar
                    // E.g.: Could not find method SomeFrameworkMethod, referenced from method
                    // Foo.bar
                    // E.g.: The member "Foo.bar"
                    // E.g.: The class "Foobar"
                    // Be careful about matching %c without %m since language tags look like class
                    // names.
                    + "(?:.*?%c\\.%m)|"
                    + "(?:.*?\"%c\\.%m\".*)|"
                    + "(?:.*\\b(?:[Cc]lass|[Tt]ype)\\b.*?\"%c\".*)|"
                    + "(?:.*\\b(?:[Cc]lass|[Tt]ype)\\b.*?%c)|"
                    // E.g.: java.lang.RuntimeException: Intentional Java Crash
                    + "(?:%c:.*)|"
                    // See if entire line matches a class name (e.g. for manual deobfuscation)
                    + "(?:%c)"
                    + ")";

    private static void usage() {
        System.err.println("Usage: echo $OBFUSCATED_CLASS | java_deobfuscate Foo.apk.mapping");
        System.err.println("Usage: java_deobfuscate Foo.apk.mapping < foo.log");
        System.err.println(
                "Note: Deobfuscation of symbols outside the context of stack "
                        + "traces will work only when lines match the regular expression defined "
                        + "in FlushingReTrace.java.");
        System.err.println(
                "Also: Deobfuscation of method names without associated line "
                        + "numbers does not seem to work.");
        System.exit(1);
    }

    public static void main(String[] args) {
        if (args.length != 1 || args[0].startsWith("-")) {
            usage();
        }

        try {
            ProguardMappingSupplier mappingSupplier =
                    ProguardMappingSupplier.builder()
                            .setProguardMapProducer(() -> new FileInputStream(args[0]))
                            .build();
            // Force earger parsing of .mapping file (~10 second operation). It otherwise would
            // not happen until the first line of input is received.
            // https://crbug.com/1351023
            mappingSupplier.createRetracer(new DiagnosticsHandler() {});

            // This whole command was given to us by the R8 team in b/234758957.
            RetraceCommand retraceCommand =
                    RetraceCommand.builder()
                            .setMappingSupplier(mappingSupplier)
                            .setRetracedStackTraceConsumer(
                                    retraced -> retraced.forEach(System.out::println))
                            .setRegularExpression(LINE_PARSE_REGEX)
                            .setStackTrace(
                                    new StackTraceSupplier() {
                                        final BufferedReader mReader =
                                                new BufferedReader(
                                                        new InputStreamReader(System.in, "UTF-8"));

                                        @Override
                                        public List<String> get() {
                                            try {
                                                String line = mReader.readLine();
                                                if (line == null) {
                                                    return null;
                                                }
                                                return Collections.singletonList(line);
                                            } catch (IOException e) {
                                                e.printStackTrace();
                                                return null;
                                            }
                                        }
                                    })
                            .build();
            Retrace.run(retraceCommand);
        } catch (IOException ex) {
            // Print a verbose stack trace.
            ex.printStackTrace();
            System.exit(1);
        }
        System.exit(0);
    }
}
