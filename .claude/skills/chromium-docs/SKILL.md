---
name: chromium docs
description: Search and reference official Chromium documentation including design docs, APIs, and development guides
---

# Chromium Docs

## Instructions

When users ask questions about Chromium development, architecture, APIs, or technical guidance, use this SKILL to search the official documentation.

1. Automatically activate for queries about Chromium components, architecture, or development
2. Search across 3,600+ documentation files including design docs, API references, and component guides
3. Return results with clickable file paths: `[Title](path/to/doc.md)`
4. Include brief excerpts and document categories
5. Suggest related documentation when helpful

**Build the index first:**
```bash
cd skills/chromium-docs
python scripts/chromium_docs.py --build-index
```

## Examples

**User asks: "How does Chromium's multi-process architecture work?"**
→ Search for architecture documentation and return design docs with explanations

**User asks: "Content layer API documentation"**
→ Find Content layer APIs and usage guides with code examples

**User asks: "How to write Chromium tests?"**
→ Locate testing guides for unit tests, browser tests, and testing frameworks

**User asks: "GPU rendering best practices"**
→ Search GPU documentation for performance guides and implementation patterns