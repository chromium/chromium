---
name: chromium-docs
description: >-
  Search and reference Chromium documentation from the local docs index,
  including design docs, APIs, and development guides. Use when the user asks
  to find, locate, browse, or learn from Chromium docs about architecture,
  APIs, testing, GPU, networking, or other Chromium topics.
---

# Chromium Documentation Search

## When to activate

Activate this skill when the user:

- Asks to **find or locate** Chromium documentation (e.g. "where are the Mojo
  docs?", "find the site-isolation design doc")
- Asks **how to learn/use/understand** a Chromium subsystem or concept and
  expects documentation references (e.g. "how to learn mojom", "how to
  understand site isolation docs")
- Needs **documentation links** for a component or subsystem (e.g. "GPU docs",
  "network stack references")
- Wants to **browse** what documentation exists for a topic or category

Do **NOT** activate when:

- The user asks to implement, modify, or debug code without requesting
  documentation references.
- The user already provided an exact file path and only wants the file
  content explained.
- The request is unrelated to Chromium documentation lookup (for example,
  general programming Q&A).
- The user asks for build/test execution only and does not need supporting docs.

## Usage

**Build the index (required before first search; rebuild after major syncs):**

```bash
python ../../../agents/skills/chromium-docs/scripts/chromium_docs.py \
  --build-index
```

**Search documentation:**

```bash
python ../../../agents/skills/chromium-docs/scripts/chromium_docs.py \
  "your query"
```

## Index maintenance

The index should be rebuilt when:

- **First use** — no index exists yet
- **After a major `git pull` or rebase** — new docs may have been added
- **Search results seem stale or incomplete**

The index covers ~2000+ markdown files and builds in about 30 seconds.

## Error handling

If the search returns "index needs to be built first":

1. Run the `--build-index` command shown above
2. Retry the original search

## Available categories

Documents are classified into the following categories. Use these names to
understand result groupings:

| Category | Covers |
|----------|--------|
| android | Android-specific code and build docs |
| ios | iOS-specific docs |
| chromeos | ChromeOS / Ash docs |
| gpu | Graphics, WebGL, Vulkan, OpenGL |
| media | Audio, video, codecs |
| security | Sandbox, site-isolation, crypto, CORS |
| network | Net stack, QUIC, TCP, DNS, SSL/TLS |
| testing | Unit tests, browser tests, test frameworks |
| ui | Views, Aura, UI toolkit |
| accessibility | a11y, screen readers |
| build | GN, Ninja, compilation |
| performance | Benchmarks, memory, speed |
| api | Mojo/mojom interfaces |
| architecture | Design documents, multi-process model |
| development | DevTools, debugging, tools |
| general | Everything else |

## Interpreting results

Search results are returned as markdown with:

- Numbered entries with linked file paths: **`[Title](path/to/doc.md)`**
- A category label (e.g. *Architecture*, *Testing*, *Network*)
- A brief excerpt or summary showing matching context

When presenting results to the user:

1. Show the **top 3-5 most relevant** results
2. Include the file path so the user can open or read the document directly
3. Briefly note what each document covers, based on the excerpt
4. If no results are found, suggest alternative search terms or a different
   category

## Examples

**User asks: "How does Chromium's multi-process architecture work?"**
> Search `"multi-process architecture"` and return the top design docs with
> file paths and summaries.

**User asks: "Find all testing-related docs"**
> Search `"testing guide"` to surface testing guides and frameworks.

**User asks: "Where are the Mojo IPC docs?"**
> Search `"mojo ipc"` and return linked paths to the Mojo binding and
> interface documentation.

**User asks: "GPU rendering best practices"**
> Search `"gpu rendering"` and present GPU-category results with excerpts.

## Resources

- **Search config**:
  `../../../agents/skills/chromium-docs/data/configs/search_config.json`
- **README**: `../../../agents/skills/chromium-docs/README.md`
- **Index scope**: All `.md` files matching `docs/**/*.md`,
  `*/README.md`, and `*/docs/*.md` across the entire Chromium source tree
