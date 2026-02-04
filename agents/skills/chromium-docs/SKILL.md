---
name: chromium-docs
description: >-
  Search and reference official Chromium documentation including design docs,
  APIs, and development guides. Use when looking for docs about architecture,
  APIs, testing, GPU, networking, or other Chromium topics.
---

# Chromium Documentation Search

## Instructions

When users ask questions about Chromium development, architecture, APIs, or
technical guidance:

1. Automatically activate for queries about Chromium components, architecture,
   or development
2. Search chromium documentation files including design docs, API references,
   and component guides
3. Return results with clickable file paths: `[Title](path/to/doc.md)`
4. Include brief excerpts and document categories
5. Suggest related documentation when helpful

## Usage

Run the following commands from the agent skill directory
(e.g. `.claude/skills/chromium-docs/`).

**Build the index first (one-time setup):**

```bash
python ../../../agents/skills/chromium-docs/scripts/chromium_docs.py --build-index
```

**Search documentation:**

```bash
python ../../../agents/skills/chromium-docs/scripts/chromium_docs.py "your query"
```

## Examples

**User asks: "How does Chromium's multi-process architecture work?"**
→ Search for architecture documentation and return design docs with explanation

**User asks: "Content layer API documentation"**
→ Find Content layer APIs and usage guides with code examples

**User asks: "How to write Chromium tests?"**
→ Locate testing guides for unit tests, browser tests, and testing frameworks

**User asks: "GPU rendering best practices"**
→ Search GPU documentation for performance guides and implementation patterns

## More Information

See `../../../agents/skills/chromium-docs/README.md` for configuration and
detailed documentation.
