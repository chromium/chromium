---
name: chrome-components
description: Use this skill when you need to map components across platforms, reference Chrome Design System component data, or generate a component spec.
---

# Chrome Components

Chrome components are the basic building blocks that make up the Chrome UI.

## Component map

The component map ([components.md](./references/components.md)) lists all
supported Chrome Design System components, each mapped to equivalent components
across all Chrome platforms.

For example, to find what a Figma `Checkbox` maps to in WebUI, find `Checkbox`
under the **Figma Component** column and then in that row, see the value under
the **WebUI** column (`<cr-checkbox>`).

## Component specs

Each component has its own spec file that contains detailed specifications about
the component.

To find a component spec, find the component in the
[component map](./references/components.md) and then follow the link to that
component's spec. Alternatively, find the component by its name in the
[./references/component-specs/](./references/component-specs/) directory.

## Generating a component spec

To generate a component spec, reference
[generate-component-spec.md](./references/generate-component-spec.md). Currently
this requires a link to a Figma component as the starting point.
