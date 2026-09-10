---
name: figma-context
description: Use this skill when you need to extract design context, metadata, tokens, and component mappings from a Figma design URL or node ID.
metadata:
  author: Chrome Design System Team
compatibility: Requires Figma MCP Server to be running
---

# Figma Context: Discovery & Retrieval

When given a Figma design URL (e.g.,
`https://www.figma.com/design/:fileKey/:fileName?node-id=:nodeId`):

1. **Extract Parameters**: Extract the `fileKey` and the `nodeId` (replace
   hyphens with colons, e.g., `128-1951` becomes `128:1951`).
2. **Retrieve Design Context**: Call `get_design_context` with `fileKey` and
   `nodeId` to fetch the metadata, layout hierarchy, layer styles, and text
   annotations of the frame.
