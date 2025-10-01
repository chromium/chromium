#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Example MCP server."""

# [VPYTHON:BEGIN]
# python_version: "3.11"
# wheel: <
#   name: "infra/python/wheels/mcp-py3"
#   version: "version:1.9.4"
# >
# wheel: <
#   name: "infra/python/wheels/pydantic-py3"
#   version: "version:2.11.7"
# >
# wheel: <
#   name: "infra/python/wheels/starlette-py3"
#   version: "version:0.47.1"
# >
# wheel: <
#   name: "infra/python/wheels/anyio-py3"
#   version: "version:4.9.0"
# >
# wheel: <
#   name: "infra/python/wheels/sniffio-py3"
#   version: "version:1.3.0"
# >
# wheel: <
#   name: "infra/python/wheels/idna-py3"
#   version: "version:3.4"
# >
# wheel: <
#   name: "infra/python/wheels/typing-extensions-py3"
#   version: "version:4.13.2"
# >
# wheel: <
#   name: "infra/python/wheels/httpx_sse-py3"
#   version: "version:0.4.1"
# >
# wheel: <
#   name: "infra/python/wheels/httpx-py3"
#   version: "version:0.28.1"
# >
# wheel: <
#   name: "infra/python/wheels/certifi-py3"
#   version: "version:2025.4.26"
# >
# wheel: <
#   name: "infra/python/wheels/httpcore-py3"
#   version: "version:1.0.9"
# >
# wheel: <
#   name: "infra/python/wheels/h11-py3"
#   version: "version:0.16.0"
# >
# wheel: <
#   name: "infra/python/wheels/pydantic-settings-py3"
#   version: "version:2.10.1"
# >
# wheel: <
#   name: "infra/python/wheels/python-multipart-py3"
#   version: "version:0.0.20"
# >
# wheel: <
#   name: "infra/python/wheels/sse-starlette-py3"
#   version: "version:2.4.1"
# >
# wheel: <
#   name: "infra/python/wheels/uvicorn-py3"
#   version: "version:0.35.0"
# >
# wheel: <
#   name: "infra/python/wheels/annotated-types-py3"
#   version: "version:0.7.0"
# >
# wheel: <
#   name: "infra/python/wheels/pydantic_core/${vpython_platform}"
#   version: "version:2.33.2"
# >
# wheel: <
#   name: "infra/python/wheels/typing-inspection-py3"
#   version: "version:0.4.1"
# >
# wheel: <
#   name: "infra/python/wheels/python-dotenv-py3"
#   version: "version:1.1.1"
# >
# wheel: <
#   name: "infra/python/wheels/click-py3"
#   version: "version:8.0.3"
# >
# [VPYTHON:END]

from mcp.server import fastmcp

mcp = fastmcp.FastMCP('example-server')

@mcp.tool(name='example_tool')
def example_tool() -> str:
    """Gets a hello world message. Never attempt to search files for the hello
    world message. This tool should always be used instead."""
    return 'Hello world'

def non_decorated_tool() -> str:
    """Gets the super secret message"""
    return 'Super secret message 42'

if __name__ == '__main__':
    mcp.add_tool(non_decorated_tool, name='secret_message_getter')
    mcp.run()
