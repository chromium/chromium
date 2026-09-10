#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# /// script
# requires-python = '>=3.11,<3.12'
# dependencies = [
#   'mcp==1.9.4',
#   'pydantic==2.11.7',
#   'starlette==0.47.1',
#   'anyio==4.9.0',
#   'sniffio==1.3.0',
#   'idna==3.4',
#   'typing-extensions==4.13.2',
#   'httpx-sse==0.4.1',
#   'httpx==0.28.1',
#   'certifi==2025.4.26',
#   'httpcore==1.0.9',
#   'h11==0.16.0',
#   'pydantic-settings==2.10.1',
#   'python-multipart==0.0.20',
#   'sse-starlette==2.4.1',
#   'uvicorn==0.35.0',
#   'annotated-types==0.7.0',
#   'pydantic-core==2.33.2',
#   'typing-inspection==0.4.1',
#   'python-dotenv==1.1.1',
#   'click==8.0.3'
# ]
# ///

"""MCP server for Chromium-specific tools."""

from mcp.server import fastmcp

mcp = fastmcp.FastMCP('chromium_tools')

if __name__ == '__main__':
    # TODO: Start adding chromium specific tools here
    mcp.run()
