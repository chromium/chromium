# Project Knowledge

An agent skill that tells agents where to look for knowledge of the design system. This is meant to be the canonical location of the "data bridge". The purpose of the data bridge is to give LLMs understanding of Chrome Design System knowledge.

## Usage

Issue prompt your LLM to reference the Chrome Design System token map.

## Notes and caveats

- Currently the skill only contains the token map. Component specs should be moved so there are located here also.
- Still considering the file architecture (there doesn't seem to be an industry standard yet). It may be better to separate the skill from the data (put data into a `shared-assets` folder that is sibling to `skills` folder).
