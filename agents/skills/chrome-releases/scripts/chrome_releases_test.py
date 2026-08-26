#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for chrome_releases script."""

import argparse
import io
import json
import unittest
from unittest import mock
import urllib.error

import chrome_releases


class MainTest(unittest.TestCase):
    @mock.patch('chrome_releases.query_api')
    def test_commit_success(self, mock_query: mock.MagicMock) -> None:
        mock_query.return_value = ({'commitPosition': 12345}, None)
        with mock.patch('sys.stdout', new_callable=io.StringIO) as mock_out:
            ret = chrome_releases.main(['commit', 'e4b52fce'])
            self.assertEqual(ret, 0)
            mock_query.assert_called_once_with('commits/e4b52fce')
            self.assertEqual(
                json.loads(mock_out.getvalue()), {'commitPosition': 12345}
            )

    @mock.patch('chrome_releases.query_api')
    def test_milestone_success(self, mock_query: mock.MagicMock) -> None:
        mock_query.return_value = ({'branches': []}, None)
        with mock.patch('sys.stdout', new_callable=io.StringIO) as mock_out:
            ret = chrome_releases.main(['milestone', '136'])
            self.assertEqual(ret, 0)
            mock_query.assert_called_once_with('milestones/136')
            self.assertEqual(json.loads(mock_out.getvalue()), {'branches': []})

    @mock.patch('chrome_releases.query_api')
    def test_version_success(self, mock_query: mock.MagicMock) -> None:
        mock_query.return_value = ({'milestone': 136}, None)
        with mock.patch('sys.stdout', new_callable=io.StringIO) as mock_out:
            ret = chrome_releases.main(['version', '136.0.7051.0'])
            self.assertEqual(ret, 0)
            mock_query.assert_called_once_with(
                'products/chrome/versions/136.0.7051.0'
            )
            self.assertEqual(
                json.loads(mock_out.getvalue()), {'milestone': 136}
            )

    @mock.patch('chrome_releases.query_api')
    def test_version_custom_product_and_alias(
        self, mock_query: mock.MagicMock
    ) -> None:
        mock_query.return_value = ({'version': 'latest-main'}, None)
        with mock.patch('sys.stdout', new_callable=io.StringIO):
            ret = chrome_releases.main(
                ['version', 'latest-main', '--product', 'v8']
            )
            self.assertEqual(ret, 0)
            mock_query.assert_called_once_with(
                'products/v8/versions/latest-main'
            )

    @mock.patch('chrome_releases.query_api')
    def test_api_error(self, mock_query: mock.MagicMock) -> None:
        mock_query.return_value = (None, 'HTTP 404: Not Found')
        with mock.patch('sys.stderr', new_callable=io.StringIO) as mock_err:
            ret = chrome_releases.main(['commit', 'invalid'])
            self.assertEqual(ret, 1)
            self.assertIn('HTTP 404: Not Found', mock_err.getvalue())


class GetResourceNameTest(unittest.TestCase):
    def test_commit(self) -> None:
        args = argparse.Namespace(command='commit', commit_hash='e4b52fce')
        self.assertEqual(
            chrome_releases.get_resource_name(args), 'commits/e4b52fce'
        )

    def test_milestone(self) -> None:
        args = argparse.Namespace(command='milestone', milestone='136')
        self.assertEqual(
            chrome_releases.get_resource_name(args), 'milestones/136'
        )

    def test_version_default_product(self) -> None:
        args = argparse.Namespace(
            command='version', product='chrome', version='136.0.7051.0'
        )
        self.assertEqual(
            chrome_releases.get_resource_name(args),
            'products/chrome/versions/136.0.7051.0',
        )

    def test_version_custom_product(self) -> None:
        args = argparse.Namespace(
            command='version', product='v8', version='latest-main'
        )
        self.assertEqual(
            chrome_releases.get_resource_name(args),
            'products/v8/versions/latest-main',
        )

    def test_unknown_command(self) -> None:
        args = argparse.Namespace(command='unknown')
        with self.assertRaises(ValueError):
            chrome_releases.get_resource_name(args)


class QueryApiTest(unittest.TestCase):
    @mock.patch('urllib.request.urlopen')
    def test_success(self, mock_urlopen: mock.MagicMock) -> None:
        mock_response = mock.MagicMock()
        mock_response.read.return_value = b'{"version": "136.0.7051.0"}'
        mock_response.__enter__.return_value = mock_response
        mock_urlopen.return_value = mock_response

        data, err = chrome_releases.query_api(
            'products/chrome/versions/136.0.7051.0'
        )
        self.assertIsNone(err)
        self.assertEqual(data, {'version': '136.0.7051.0'})

    @mock.patch('urllib.request.urlopen')
    def test_http_error(self, mock_urlopen: mock.MagicMock) -> None:
        mock_urlopen.side_effect = urllib.error.HTTPError(
            'http://test',
            404,
            'Not Found',
            {},
            io.BytesIO(b'Resource not found'),
        )
        data, err = chrome_releases.query_api('commits/invalid')
        self.assertIsNone(data)
        self.assertIn('HTTP 404', err)

    @mock.patch('urllib.request.urlopen')
    def test_generic_error(self, mock_urlopen: mock.MagicMock) -> None:
        mock_urlopen.side_effect = Exception('Connection refused')
        data, err = chrome_releases.query_api('commits/invalid')
        self.assertIsNone(data)
        self.assertIn('Connection refused', err)


if __name__ == '__main__':
    unittest.main()
