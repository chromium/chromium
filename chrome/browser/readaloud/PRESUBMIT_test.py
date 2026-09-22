#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
# chromium/src is 3 levels up from chrome/browser/readaloud
sys.path.insert(0, os.path.join(file_dir_path, '..', '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi


class AssertionOrderPresubmitTest(unittest.TestCase):

    def testValidActualExpected(self):
        lines = [
            '  EXPECT_EQ(result, 0);',
            '  EXPECT_EQ(segment->sample_rate(), 44100);',
            '  EXPECT_EQ(GetViewerHandle(), nullptr);',
            '  EXPECT_EQ(bounds.start_time, base::Milliseconds(100));',
            '  EXPECT_EQ(controller->playback_rate(), 1.0f);',
            '  EXPECT_EQ(actual_state, expected_state);',
            '  ASSERT_NE(buffer, nullptr);',
            '  EXPECT_NEAR(segment->duration().InSecondsF(), 1.5f, 0.01f);',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))

    def testInvalidExpectedActual(self):
        test_cases = [
            (
                '  EXPECT_EQ(0, result);',
                'EXPECT_EQ(0, result)',
            ),
            (
                '  EXPECT_EQ(nullptr, segment->audio_buffer());',
                'EXPECT_EQ(nullptr, segment->audio_buffer())',
            ),
            (
                '  EXPECT_EQ(0u, segments[0]->segment_index);',
                'EXPECT_EQ(0u, segments[0]->segment_index)',
            ),
            (
                '  EXPECT_EQ("Title", metadata->title());',
                'EXPECT_EQ("Title", metadata->title())',
            ),
            (
                '  EXPECT_EQ(kChannels, segment->channel_count());',
                'EXPECT_EQ(kChannels, segment->channel_count())',
            ),
            (
                '  EXPECT_EQ(base::Milliseconds(500), segment->duration());',
                'EXPECT_EQ(base::Milliseconds(500), segment->duration())',
            ),
            (
                '  EXPECT_EQ(expected_state, last_state);',
                'EXPECT_EQ(expected_state, last_state)',
            ),
            (
                '  ASSERT_EQ(nullptr, ReadAloudServiceFactory::Get(p));',
                'ASSERT_EQ(nullptr, ReadAloudServiceFactory::Get(p))',
            ),
            (
                '  EXPECT_NEAR(1.5f, segment->duration().InSecondsF(), 0.01f);',
                'EXPECT_NEAR(1.5f, segment->duration().InSecondsF(), 0.01f)',
            ),
        ]
        for line, expected_snippet in test_cases:
            mock_input = MockInputApi()
            mock_input.files = [
                MockAffectedFile(
                    'chrome/browser/readaloud/foo_unittest.cc', [line]
                )
            ]
            mock_output = MockOutputApi()
            results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
            self.assertEqual(
                1, len(results), f'Failed to catch violation for: {line}'
            )
            self.assertIn(expected_snippet, results[0].message)

    def testMultilineMacroInvocation(self):
        lines = [
            '  EXPECT_EQ(',
            '      0u,',
            '      segments[0]->segment_index);',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(1, len(results))
        self.assertIn('foo_unittest.cc:1', results[0].message)

    def testNoCheckEscapeHatch(self):
        lines = [
            '  EXPECT_EQ(0, result);  // nocheck',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))

    def testCommentedOutInvocationIgnored(self):
        lines = [
            '  // EXPECT_EQ(0, result);',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))

    def testBothConstantsAllowed(self):
        lines = [
            '  EXPECT_EQ(kFoo, kBar);',
            '  EXPECT_EQ(0, 0);',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))

    def testIgnoresNonCppFiles(self):
        lines = [
            '  assertEquals(0, result);',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/browser/readaloud/FooTest.java', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))


if __name__ == '__main__':
    unittest.main()
