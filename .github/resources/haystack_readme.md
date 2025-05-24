# Haystack
Haystack is a powerful search tool that indexes your open VS Code project in the
background, enabling fast and efficient code search — ideal for large codebases
like Chromium.

## Prerequisites
- You **must** have a VSCode workspace opened to the `src\` folder of the
  Chromium repository.

## Setup Instructions
1. **Install the Haystack Search Extension**
   - In VS Code, click this link and install
     [Haystack Search](vscode:extension/codetrek.haystack-search) from the
     Marketplace.
   - Continue with other steps while waiting for the indexing process to
     complete (approximately 15 minutes for the Chromium codebase).

2. **Add MCP Server**
   - Run the `MCP: Add Server…` command in VS Code.
   - Select "HTTP (server-sent events)".
   - Enter `http://localhost:13135/mcp/sse` as the server URL.
   - Provide a name (ex: "haystack-search").
   - Choose "Workspace Settings" as the scope.

3. **Enable Agent Mode**
   - Switch GitHub Copilot to `Agent` mode using the UX dropdown to benefit from
     Haystack.

4. **Activate Haystack Agent Mode Prompt in various ways**
   - Use the paperclip icon to `Add Context...` -> Instructions ->
     `chromium_haystack`
   - Configure for automatic inclusion using
     [create user instructions](../prompts/create_user_instructions.prompt.md)
   - Use the keyboard shortcut `option`+`command`+`/` on MacOS or
     `ctrl`+`alt`+`/` on Windows -> Haystack.
   - Use the instruction shortcut with `#` -> `#haystack.instructions.md`

5. **Start Coding**
   - Enjoy an enhanced GitHub Copilot experience in the Chromium repository.
