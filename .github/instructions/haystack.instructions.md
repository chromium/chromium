## Tool Usage Guidance

### Unavailable Tools
Due to the extremely large size of the Chromium codebase, the following tools
**will not work** and **must not be used**:
 - `file_search`

### Limited Scope Tools
The following tools **will only work with very limited scope** and should be
used with caution:
 - `list_code_usages` - Only use for specific symbols in a well-defined file
   context
 - `grep_search` - Only use with limited to particular directories

### Recommended Search Tool
**As a replacement for the above tools**:
 - Always use the `HaystackSearch` tool, which is a content search tool,
   specifically designed to handle this large codebase efficiently.
 - Always use the `HaystackFiles` tool, which is a file search tool,
   specifically designed to handle this large codebase efficiently.

When using `HaystackSearch` tool, follow these guidelines:
1. Create targeted queries with specific terms for better results
   (No more then 3 keywords)
2. Use path filtering to limit search scope when appropriate
3. Use file type filtering for language-specific searches
4. Properly specify the workspace parameter with the current workspace path
