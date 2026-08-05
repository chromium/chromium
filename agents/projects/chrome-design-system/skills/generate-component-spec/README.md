# Generate Component Spec

An agent skill that takes a URL to a Figma component from the Chrome Design System (CDS) and maps it to equivalent C++ Views and WebUI components in the Chromium codebase to generate a consolidated Markdown specification of the component.

This skill exists in the initial stage of the CDS engineering projects and will likely not stick around forever, but could be repurposed for other things in the future. The purposes of this skill at the moment are to:
- Enable AI agents to identify gaps between Figma components and their code counterparts as part of assisting the CDS Team in the initial audit of components.
- Facilitate the production of component documentation.
- Facilitate the production of files that give agents more context into the design system in order to create better prototypes and assist in reviewing designs and code to achieve greater parity.

## Prerequisites

- A running [Figma MCP server](https://help.figma.com/hc/en-us/articles/32132100833559-Guide-to-the-Figma-MCP-server) in order to give your agent the ability to read from Figma.
- A checkout of the Chromium repository (must be run in the root of the repo).

## Usage

1. Get the URL of a component's frame in Figma.
2. Pass it to the agent and ask to create a component spec.
3. A Markdown file will be created.

## Current limitations

- Only designed to handle desktop components.
- The Markdown file is created inside a child directory of this skill.
  - This is not git ignored for the purpose of sharing generated files.
  - This could change in the future when we have a better picture of where the component specs and data bridge will fit into the overall CDS toolset.
