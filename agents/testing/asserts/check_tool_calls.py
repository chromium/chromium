# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Asserts for checking tool calls."""


import re


def check_tool_used_with_args_match(_: str, context: dict) -> dict:
    """Checks if one of the given tools was used with arguments matching all of
    a list of regexes.

    The assertion should be in the format:
    - type: >
        python:path/to/this/file.py:check_tool_used_with_args_match
      config:
        args_regexes: [<regex1>, <regex2>, ...]
        tool_names: [<tool_name_1>, <tool_name_2>, ...]
    """
    config = context.get('config', {})
    args_regexes = config.get('args_regexes')
    if not args_regexes:
        return {
            'pass': False,
            'reason': ('No regexes specified in the `args_regexes` field of '
                       'the assertion config.'),
            'score': 0,
        }
    if isinstance(args_regexes, str):
        args_regexes = [args_regexes]
    tool_names = config.get('tool_names')
    if not tool_names:
        return {
            'pass': False,
            'reason': ('No tools specified in the `tool_names` field of the '
                       'assertion config.'),
            'score': 0,
        }
    if isinstance(tool_names, str):
        tool_names = [tool_names]
    tool_names = set(tool_names)

    provider_response = context.get('providerResponse', {})
    metrics = provider_response.get('metrics', {})
    tool_calls = metrics.get('tool_calls', [])

    tool_used = None
    for call in tool_calls:
        function_name = call.get('function_name')
        if function_name in tool_names:
            tool_used = function_name
            args_str = call.get('function_args', '')
            if not args_str:
                continue

            if all(re.search(regex, args_str) for regex in args_regexes):
                return {
                    'pass': True,
                    'reason': (f'Tool "{function_name}" was used with '
                               'arguments matching all regexes.'),
                    'score': 1,
                }

    if not tool_used:
        used_tool_names = {c.get('function_name') for c in tool_calls}
        return {
            'pass': False,
            'reason': (f'None of the tools {sorted(tool_names)} were used. '
                       f'Used tools: {sorted(list(used_tool_names))}'),
            'score': 0,
        }

    # A tool was used, but no arguments matched the regex.
    return {
        'pass': False,
        'reason': (f'Tool "{tool_used}" was used, but its arguments did not '
                   f'match all of the regexes: "{args_regexes}".'),
        'score': 0,
    }
