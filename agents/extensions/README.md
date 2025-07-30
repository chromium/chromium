# Gemini Extensions

This directory contains extensions / MCP ([model context protocol]) server
configurations useful for Chromium development. Each subdirectory within this
directory corresponds to one extension.

Configuration are provided in [gemini-cli extensions] format.

[model context protocol]: https://modelcontextprotocol.io/
[gemini-cli extensions]: https://github.com/google-gemini/gemini-cli/blob/main/docs/extension.md

## Managing Configurations

Use `agents/extensions/install.py` to manage extensions.

### Listing Extensions

To see a list of available extensions and their install status:

```bash
vpython3 agents/extensions/install.py list
```

### Adding Extensions

```bash
# Copies directory to //.gemini/extensions.
vpython3 agents/extensions/install.py add <extension_name_1> <extension_name_2>

# Copies directory to ~/.gemini/extensions
vpython3 agents/extensions/install.py add --global <extension_name_1> <extension_name_2> 
```

### Updating Extensions

```bash
vpython3 agents/extensions/install.py update <server_name>
```

You can also update all installed servers at once:

```bash
vpython3 agents/extensions/install.py update
```

### Removing Extensions

```bash
vpython3 agents/extensions/install.py remove <server_name>
```

## Types of MCP Servers

There are three types of MCP server configurations supported:

1. **Local MCP Server (chromium tree):** The configuration for these servers
   points to a local MCP server that is located elsewhere within the Chromium
   source tree.

2. **Local MCP Server (prebuilt):** These servers are prebuilt as CIPD packages
   and located within the same subdirectory as their configuration files.

3. **Remote MCP Server:** The configuration for these servers contains a
   reference to a remote URL where the MCP server is hosted.

## Creating an MCP server

See the [example][3] server for a minimal example for creating an MCP tool
with python and FastMCP

[1]: https://modelcontextprotocol.io/
[2]: https://github.com/google-gemini/gemini-cli/blob/main/docs/extension.md
[3]: example_server/README.md
