#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest
import zipfile

import tracereferences

_STDERR = '\n'.join(
    [
        'Warning: Missing class android.webkit.WebView (referenced from: void '
        'org.Foo.bar(android.content.Context) and 2 other contexts)',
        'Missing method void '
        'android.webkit.WebView.setWebContentsDebuggingEnabled(boolean) '
        '(referenced from: void org.Foo$$ExternalSyntheticLambda0.run() and 1 '
        'other context)',
        'Missing class android.util.StatsEvent$Builder (referenced from: void '
        'org.Baz.qux())',
    ]
)


class TraceReferencesTest(unittest.TestCase):
    def testSymbolClass(self):
        cases = [
            ('class', 'android.webkit.WebView', 'android.webkit.WebView'),
            (
                'method',
                'void android.webkit.WebView.<init>(android.content.Context)',
                'android.webkit.WebView',
            ),
            (
                'method',
                'android.webkit.WebView$HitTestResult '
                'android.webkit.WebView.getHitTestResult()',
                'android.webkit.WebView',
            ),
            (
                'field',
                'int android.webkit.WebView.RENDERER_PRIORITY_BOUND',
                'android.webkit.WebView',
            ),
            # Unexpected trailing text (e.g. if R8's output format changes).
            (
                'class',
                'android.webkit.WebView [from: org.Foo]',
                'android.webkit.WebView',
            ),
            (
                'field',
                'int android.webkit.WebView.FOO [from: org.Foo]',
                'android.webkit.WebView',
            ),
        ]
        for kind, symbol, expected in cases:
            self.assertEqual(
                tracereferences._SymbolClass(kind, symbol), expected
            )

    def testParseContext(self):
        cases = {
            'org.Foo': ('org.Foo', None),
            'org.Foo$$ExternalSyntheticLambda0': ('org.Foo', None),
            'void org.Foo$-CC.bar()': ('org.Foo', None),
            'void org.Foo.lambda$bar$0(int)': ('org.Foo', None),
            'void org.Foo.<clinit>()': ('org.Foo', None),
            'void org.Foo$Bar.<init>(int, java.lang.String)': (
                'org.Foo$Bar',
                '<init>(int, java.lang.String)',
            ),
            'boolean org.Foo.bar(android.webkit.WebView)': (
                'org.Foo',
                'boolean bar(android.webkit.WebView)',
            ),
            'android.webkit.WebView org.Foo.mWebView': (
                'org.Foo',
                'android.webkit.WebView mWebView',
            ),
        }
        for context, expected in cases.items():
            self.assertEqual(
                tracereferences._ParseContext(context), expected, msg=context
            )

    def testFindDisallowedReferences(self):
        violations = tracereferences._FindDisallowedReferences(
            _STDERR, {'android.webkit.WebView'}
        )
        self.assertEqual(
            violations,
            [
                (
                    'Missing class android.webkit.WebView (referenced from: '
                    'void org.Foo.bar(android.content.Context) and 2 other '
                    'contexts)',
                    'void org.Foo.bar(android.content.Context)',
                ),
                (
                    'Missing method void '
                    'android.webkit.WebView.setWebContentsDebuggingEnabled('
                    'boolean) (referenced from: void '
                    'org.Foo$$ExternalSyntheticLambda0.run() and 1 other '
                    'context)',
                    'void org.Foo$$ExternalSyntheticLambda0.run()',
                ),
            ],
        )

    def testCreateDisallowedReferencesMessage(self):
        violations = tracereferences._FindDisallowedReferences(
            _STDERR, {'android.webkit.WebView'}
        )
        message = tracereferences._CreateDisallowedReferencesMessage(
            violations, {'android.webkit.WebView'}
        )
        self.assertIn(
            '-whyareyoukeeping class org.Foo {\n'
            '    void bar(android.content.Context);\n'
            '}\n',
            message,
        )
        self.assertNotIn('org.Baz', message)

    def testIncludePrefixes(self):
        names = [
            'META-INF/',
            'META-INF/MANIFEST.MF',
            'android/',
            'android/app/Activity.class',
            'android/webkit/',
            'android/webkit/WebView.class',
            'android/webkit/WebView$HitTestResult.class',
            'android/webkit/internal/Foo.class',
            'java/lang/Object.class',
        ]
        self.assertEqual(
            tracereferences._IncludePrefixes(
                names, {'android/webkit/WebView.class'}
            ),
            [
                'META-INF/',
                'android/app/',
                'android/webkit/WebView$HitTestResult.class',
                'android/webkit/internal/',
                'java/',
            ],
        )

    @unittest.skipUnless(
        os.path.exists(tracereferences._SINGLEJAR_PATH), 'Requires singlejar'
    )
    def testRemoveClassesFromJars(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            jar_path = os.path.join(tmp_dir, 'android.jar')
            other_jar_path = os.path.join(tmp_dir, 'other.jar')
            with zipfile.ZipFile(jar_path, 'w') as z:
                z.writestr('android/webkit/WebView.class', b'a')
                z.writestr('android/webkit/WebView$HitTestResult.class', b'b')
            with zipfile.ZipFile(other_jar_path, 'w') as z:
                z.writestr('foo/Bar.class', b'c')

            new_paths = tracereferences._RemoveClassesFromJars(
                [jar_path, other_jar_path], {'android.webkit.WebView'}, tmp_dir
            )
            self.assertNotEqual(new_paths[0], jar_path)
            self.assertEqual(new_paths[1], other_jar_path)
            with zipfile.ZipFile(new_paths[0]) as z:
                names = z.namelist()
            self.assertIn('android/webkit/WebView$HitTestResult.class', names)
            self.assertNotIn('android/webkit/WebView.class', names)

    def testRemoveClassesFromJarsOnlyRemovedClasses(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            jar_path = os.path.join(tmp_dir, 'webview.jar')
            with zipfile.ZipFile(jar_path, 'w') as z:
                z.writestr('android/webkit/', b'')
                z.writestr('android/webkit/WebView.class', b'a')
            self.assertEqual(
                tracereferences._RemoveClassesFromJars(
                    [jar_path], {'android.webkit.WebView'}, tmp_dir
                ),
                [],
            )

    def testRemoveClassesFromJarsMissingClass(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            jar_path = os.path.join(tmp_dir, 'other.jar')
            with zipfile.ZipFile(jar_path, 'w') as z:
                z.writestr('foo/Bar.class', b'c')
            with self.assertRaisesRegex(Exception, 'not found in any library'):
                tracereferences._RemoveClassesFromJars(
                    [jar_path], {'android.webkit.WebView'}, tmp_dir
                )


if __name__ == '__main__':
    unittest.main()
