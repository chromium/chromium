# Figma to Views JSON

An agent skill that takes a URL to a Figma frame from the Figma Core UI Library
and creates a prototype-quality JSON script suitable for rendering using the
Views Canvas Example.

## Prerequisites

- A running
  [Figma MCP server](https://help.figma.com/hc/en-us/articles/32132100833559-Guide-to-the-Figma-MCP-server)
  to give your agent the ability to read from Figma.
- A checkout of the Chromium repository (must be run in the root of the repo).

## Usage

1. Get the URL of a design's frame in Figma.
2. Pass it to the agent and ask to generate a JSON Script from the design.
3. JSON script files will be created in `out/<ComponentName>/` following native
   Views layout and token standards.

## Notes and caveats

- Only designed to handle standard desktop Views components.
- The files are saved to a child directory of this skill. This could change in
  the future when we have a better picture of where Figma-to-XXX generation will
  fit into the overall CDS toolset.
