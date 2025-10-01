# Creating MCP tools

This guide will walk you through adding MCP tools to an existing server or
creating a new server. Tools should be reviewed by security which can make
adding tools to an existing on a preferred option to make the review easier.

If you want to add a non-chromium specific tool to the existing
[depot_tools/mcp][5] server or a chromium specific tool to [chromium_tools][6],
see "Adding tools to the server" below.

## Creating an MCP server

This guide will walk you through implementing a [FastMCP][1] server with
vpython in chromium. The example files are contained in this same folder for
reference.

### Add the wheels to your vpython spec

The [vpython3 spec][2] should include the following fastmcp wheel for version
1.9.4 and its dependencies (either as independent .vpython3 file or part of an
embedded spec like in [server.py][3].

### Create the actual server

Import and create a server. This will hold all the tools provided by this
process:
```
from mcp.server import fastmcp

mcp = fastmcp.FastMCP('example-server')
```

## Adding tools to the server

FastMCP will interpret the types from the annotations and will use the
docstrings to provide context to the agent. If the agent is misusing the tool,
the docstring likely needs updating. Each tool can then be included in the
server either via the decorator or by calling add_tool (add_tool can be used to
conditionally include tools based on flags, saving context tokens on
unnecessary or unsupported tools).

### Decorator:
```
@mcp.tool(name='example_tool')
def example_tool() -> str:
    """Gets a hello world message. Never attempt to search files for the hello
    world message. This tool should always be used instead.""
    return 'Hello world'
```

### add_tool:

```
def non_decorated_tool() -> str:
    """Gets the super secret message"""
    return 'Super secret message 42'

mcp.add_tool(non_decorated_tool, name='secret_message_getter')
```

The last step is to call mcp.run to start the server:

```
mcp.run()
```

## Configure gemini-cli

The server should be ready to include in gemini-cli (or other agents). To
start the local server on gemini startup, the command needs to be included
either directly in your settings.json or a gemini-extension.json. This folder
also includes a server management tool for combining servers. This json can
include custom args such as flags for which tools to include.

### gemini-extension.json

If the server is being built for chromium and included in this folder, the
[install.py][4] script can be used to manage installing the server. A
gemini-extension.json file including similar information will make the server
available to install:

```
{
  "name": "example_server",
  "version": "1.0.0",
  "mcpServers": {
    "example_server": {
      "command": "vpython3",
      "args": ["agents/mcp/example_server/server.py"]
    }
  }
}
```

The tool can also be included directly in your gemini settings.json file
located in your user/.gemini/settings.json file or the local workspace. To
include the new server, either append or create the "mcpServers" section to
include the new server and command to start it. Ideally these will be included
as gemini-extension.json, however.

These servers can be temporarily disabled by prefixing "//" to the server name.
e.g. "//example_server"

## Testing

After being installed, gemini-cli should recognize the tool on startup. Start
gemini-cli. If the MCP runs and is installed correctly, the tool should be
listed under a `/mcp` call or listed after ctrl+t. In the case the tool fails
to load or communicate, ctrl+o will give some limited error information. Note
that the command to start the server is relative to where gemini was started.
The example assumes gemini was started from the chromium/src folder. Asking
gemini to run the tool outside of yolo mode should cause gemini to request
permission before calling the tool. e.g. Asking gemini "What's the secret
message?" is in this example results in:

```
 ╭─────────────────────────────────────────────────────────────────────────────────────────────╮
 │ ?  secret_message_getter (example_server MCP Server) {} ←                                   │
 │                                                                                             │
 │   MCP Server: example_server                                                                │
 │   Tool: secret_message_getter                                                               │
 │                                                                                             │
 │ Allow execution of MCP tool "secret_message_getter" from server "example_serve…             │
 │                                                                                             │
 │ ● 1. Yes, allow once                                                                        │
 │   2. Yes, always allow tool "secret_message_getter" from server "example_serve…             │
 │   3. Yes, always allow all tools from server "example_server"                               │
 │   4. No (esc)                                                                               │
 │                                                                                             │
 ╰─────────────────────────────────────────────────────────────────────────────────────────────╯
```

Which results in gemini pulling the returned message from the MCP server: "The
secret message is 42." See the [example][3] for a full working example of a
barebones MCP tool.

[1]: https://gofastmcp.com/getting-started/welcome
[2]: https://chromium.googlesource.com/infra/infra/+/HEAD/doc/users/vpython.md
[3]: server.py
[4]: ../install.py
[5]: https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:mcp/
[6]: ../chromium_tools