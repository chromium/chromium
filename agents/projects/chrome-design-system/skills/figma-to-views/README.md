# Figma to Views

An agent skill that takes a URL to a Figma frame from the Chrome Design System (CDS) and creates a production-quality C++ Views implementation.

## Prerequisites

- A running [Figma MCP server](https://help.figma.com/hc/en-us/articles/32132100833559-Guide-to-the-Figma-MCP-server) to give your agent the ability to read from Figma.
- A checkout of the Chromium repository (must be run in the root of the repo).

## Usage

1. Get the URL of a design's frame in Figma.
2. Pass it to the agent and ask to generate a C++ Views page or component from the design.
3. Native C++ Views header (`.h`) and implementation (`.cc`) files will be created in `out/<ComponentName>/` following native Views layout and token standards.

## Notes and caveats

- Only designed to handle desktop components.
- The files are saved to a child directory of this skill. This could change in the future when we have a better picture of where Figma-to-code generation will fit into the overall CDS toolset.
