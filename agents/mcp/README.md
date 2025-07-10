# MCP Server Configurations

This directory contains a collection of MCP ([model context protocol][1]) server
configurations and prebuilt MCP servers useful for Chromium development. Each
subdirectory within this directory corresponds to a single MCP server.

Configurations are provided in [gemini-cli extensions][2] format.

## Server Types

There are three types of MCP server configurations supported:

1. **Local MCP Server (chromium tree):** The configuration for these servers
   points to a local MCP server that is located elsewhere within the Chromium
   source tree.

2. **Local MCP Server (prebuilt):** These servers are prebuilt as CIPD packages
   and located within the same subdirectory as their configuration files.

3. **Remote MCP Server:** The configuration for these servers contains a
   reference to a remote URL where the MCP server is hosted.

## Using with gemini-cli

MCP servers can be configured globally or at a project level. If done globally,
they can be added to `$HOME/.gemini/extensions` and if done for your current
checkout they can go in `.gemini/extensions` of the directory from which you
invoke gemini-cli (e.g. from the `chromium/src` root).

[1] https://modelcontextprotocol.io/
[2] https://github.com/google-gemini/gemini-cli/blob/main/docs/extension.md
