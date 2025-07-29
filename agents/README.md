# Chromium Coding Agents

This directory provides a centralized location for files related to AI coding
agents (e.g. `gemini-cli`) used for development within the Chromium source tree.

The goal is to provide a scalable and organized way to share prompts and tools
among developers, accommodating the various environments (Linux, Mac, Windows)
and agent types in use.

## Directory Structure

### Prompts

Shared `GEMINI.md` prompts. See [`//agents/prompts/README.md`].

[`//agents/prompts/README.md`]: /agents/prompts/README.md

### MCPs (Model Context Protocols)

Chrome-approved MCP servers. See [`//agents/mcp/README.md`].

Use `agents/mcp/install.py` to list and configure available servers.

[`//agents/mcp/README.md`]: /agents/mcp/README.md

### Extensions

Similar to `mcp`, but not a server.

## Contributing

Please freely add self-contained task prompts and prompt templates that match
the format of the existing examples.

New MCP server configurations should be for owned and supported MCP servers and
include OWNERS.

Changes to `common.GEMINI.md` should be done carefully as it's meant to be used
broadly.
