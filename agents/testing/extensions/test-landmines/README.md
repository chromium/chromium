# Test Landmines Extension

## Purpose

This extension acts as a safeguard during automated testing of Gemini prompts
and tools. It prevents the model from making permanent changes to the Chromium
repository by blocking tools that upload code for review.

Specifically, it disables:
- The `upload_change_list` tool from the `depot_tools` MCP server.
- The `git cl upload` shell command.
- The `git push` shell command.

This extension is intended to be used in conjunction with the primary `landmines`
extension, which disables other potentially harmful or non-hermetic operations.

## Usage

This extension should be automatically loaded by the prompt evaluation test
runner during all test executions. It is not intended for general development
use. By ensuring tests cannot create or modify code reviews, it helps maintain
a clean and predictable testing environment.
