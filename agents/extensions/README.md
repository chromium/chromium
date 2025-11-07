# Gemini Extensions

This directory contains extensions / MCP ([model context protocol]) server
configurations useful for Chromium development. Each subdirectory within this
directory corresponds to one extension.

Configuration are provided in [gemini-cli extensions] format.

[model context protocol]: https://modelcontextprotocol.io/
[gemini-cli extensions]: https://github.com/google-gemini/gemini-cli/blob/main/docs/extension.md

## Managing Configurations

Use `agents/extensions/install.py` to manage extensions. This script is a
wrapper around the `gemini extensions` commands.

### Migrating from Project-Level Extensions

If you have previously installed extensions at the project-level, you will be
prompted to run the `fix` command to migrate them to the new user-level model:

```bash
vpython3 agents/extensions/install.py fix
```

### Listing Extensions

To see a list of available extensions and their install status:

```bash
vpython3 agents/extensions/install.py list
```

The output table includes the following columns:

*   **EXTENSION**: The name of the extension.
*   **AVAILABLE**: The version of the extension found in the Chromium source tree. A `-` indicates it's not available locally.
*   **INSTALLED**: The version of the extension currently installed in your Gemini CLI user-level extensions directory. A `-` indicates it's not installed.
*   **LINKED**: Indicates if the installed extension is a symbolic link (`yes`) to the source directory or a copy (`no`).
*   **ENABLED**: Indicates if the extension is enabled for the current workspace (`workspace`) or not (`-`).

### Adding Extensions

By default, extensions are installed as links in your user-level extension
directory (`~/.gemini/extensions`).

```bash
vpython3 agents/extensions/install.py add <extension_name_1> <extension_name_2>
```

To copy the extension directory instead of creating a link, use the `--copy`
flag:

```bash
vpython3 agents/extensions/install.py add --copy <extension_name_1>
```

### Updating Extensions

```bash
vpython3 agents/extensions/install.py update <extension_name>
```

You can also update all installed extensions at once:

```bash
vpython3 agents/extensions/install.py update
```

### Removing Extensions

```bash
vpython3 agents/extensions/install.py remove <extension_name>
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
