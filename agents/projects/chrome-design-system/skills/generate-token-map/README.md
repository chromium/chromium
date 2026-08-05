# Generate Token Map

An agent skill that maps Figma variables to equivalent C++ and CSS variables and generates a markdown file of this token map. The purpose of the token map is to give LLMs information about how Figma variables map to variables/tokens in code.

## Notes and caveats

- The file [figma-variables.md](./figma-variables.md) lists and categorizes all Figma variables, eliminating the need for this skill to read from Figma and make sense of the variables. While this makes token map output more deterministic, it also means if variables change in Figma, those changes will not be picked up. This will be addressed once the CDS data pipeline (for keeping things in sync) is matured.
- Running this skill will overwrite the existing `tokens.md` file. This could be updated in the future to update only what's necessary.
