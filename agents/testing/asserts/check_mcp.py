# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Asserts for checking MCP tools."""

import json
import os
import subprocess


class McpClient():

    def __init__(self, server_path):
        self.process = subprocess.Popen(  # pylint: disable=R1732
            [server_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True)
        # Initialize the server
        initialize_request = {
            'jsonrpc': '2.0',
            'method': 'initialize',
            'params': {
                'protocolVersion': '2.0',
                'processId': os.getpid(),
                'clientInfo': {
                    'name': 'mcp_client.py',
                    'version': '0.1'
                },
                'capabilities': {}
            },
            'id': 1
        }
        self.process.stdin.write(json.dumps(initialize_request) + '\n')
        self.process.stdin.flush()
        self.process.stdout.readline()

        # Send initialized notification
        initialized_notification = {
            'jsonrpc': '2.0',
            'method': 'notifications/initialized',
            'params': {}
        }
        self.process.stdin.write(json.dumps(initialized_notification) + '\n')
        self.process.stdin.flush()

    def __del__(self):
        self.process.terminate()

    def call(self, method, params=None):
        request = {
            'jsonrpc': '2.0',
            'method': 'tools/call',
            'params': {
                'name': method,
                'params': params or {},
            },
            'id': 1
        }

        self.process.stdin.write(json.dumps(request) + '\n')
        self.process.stdin.flush()

        response_str = self.process.stdout.readline()
        response = json.loads(response_str)

        if 'error' in response:
            raise RuntimeError(f'Error from server: {response["error"]}')
        if response['result'].get('isError', False):
            raise RuntimeError('The tool call failed')
        return response['result']

    def call_text(self, tool) -> str:
        mcp_response = self.call(tool)
        contents = mcp_response.get('content', [])
        if len(contents) == 0:
            raise RuntimeError('The MCP server did not respond with content')

        content_type = contents[0]['type']
        if content_type != 'text':
            raise RuntimeError(
                f'The MCP responded with an unexpected type: {content_type}')
        content_text = contents[0]['text']
        return content_text


def check_response_contains_mcp_response(agent_response: str, context):
    """Checks that the agent's response contains an MCP tool response"""
    config = context.get('config', {})
    if 'server_path' not in config or 'tool' not in config:
        raise RuntimeError(
            'The assertion is not correctly configured. Please provide '
            'both a server_path and tool.')
    server_path = config.get('server_path')
    tool = config.get('tool')
    server = McpClient(server_path)
    case_sensitive = config.get('case_sensitive', False)
    content_text = server.call_text(tool)
    if not case_sensitive:
        content_text = content_text.lower()
        agent_response = agent_response.lower()
    return {
        'pass': content_text in agent_response,
        'reason': f'"{content_text}" was in the response: "{agent_response}".',
        'score': 1 if content_text in agent_response else 0
    }
