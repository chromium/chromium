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

## Managing Configurations with `install.py`

The `install.py` script helps you manage the MCP server configurations in your
local and global Gemini CLI extension directories.

### List Servers

To see a list of all available server configurations and where they are
installed, run the `list` command (or run the script with no command):

```bash
vpython3 agents/mcp/install.py list
```

### Add (Install) a Server

To add a server configuration to your project-level extensions, use the `add`
command:

```bash
vpython3 agents/mcp/install.py add <server_name_1> <server_name_2>
```

To install to the global extensions directory (`~/.gemini/extensions`), use the
`--global` (or `-g`) flag:

```bash
vpython3 agents/mcp/install.py add --global <server_name>
```

### Update Servers

To update an installed server configuration to the latest version from the
source tree, use the `update` command:

```bash
vpython3 agents/mcp/install.py update <server_name>
```

You can also update all installed servers at once:

```bash
vpython3 agents/mcp/install.py update
```

### Remove a Server

To remove a server configuration, use the `remove` command:

```bash
vpython3 agents/mcp/install.py remove <server_name>
```

[1] https://modelcontextprotocol.io/
[2] https://github.com/google-gemini/gemini-cli/blob/main/docs/extension.md