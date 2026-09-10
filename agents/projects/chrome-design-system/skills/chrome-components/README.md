# Chrome Components

The purposes of this skill are to:

- Enable LLMs to understand Chrome components and map components cross-platform.
- Enable LLMs to identify gaps between Figma components and their code
  counterparts in order to eventually close those gaps and increase parity.
- Facilitate the production of component documentation.
- Facilitate the production of files that give agents more context into the
  design system in order to create better prototypes and assist in reviewing
  designs and code to achieve greater parity.

## Generating component specs

### Prerequisites

- A running
  [Figma MCP server](https://help.figma.com/hc/en-us/articles/32132100833559-Guide-to-the-Figma-MCP-server)
  in order to give your agent the ability to read from Figma.
- A checkout of the Chromium repository (must be run in the root of the repo).

### Usage

1. Get the URL of a component's frame in Figma.
2. Pass it to the agent and ask to create a component spec.
3. A Markdown file will be created.

## Notes and caveats

- Because of the disparity between components across platforms, components do
  not currently map 1:1 across platforms. A certain Figma component might be
  "equivalent" to two distinct components in Views.
