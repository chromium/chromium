# Chromium Documentation SKILL

A Claude Code skill that provides intelligent search across Chromium's official documentation.

## Setup

1. Build the documentation index:
```bash
cd agents/skills/chromium-docs
python scripts/chromium_docs.py --build-index
```

2. The SKILL will automatically activate for Chromium-related queries.

## Usage

Ask questions about Chromium development and the SKILL will search official documentation:

- "How does Chromium's multi-process architecture work?"
- "Content layer API documentation"
- "How to write Chromium tests?"
- "GPU rendering best practices"

## File Structure

```
agents/skills/chromium-docs/
├── OWNERS                      # Code ownership
├── SKILL.md                    # SKILL definition (source)
├── README.md                   # This file
├── .gitignore                  # Excludes generated data
├── scripts/
│   └── chromium_docs.py        # Main search implementation
└── data/
    └── configs/
        └── search_config.json  # Search configuration
```

To use this skill, symlink or copy SKILL.md to your agent's skills directory (e.g., `.claude/skills/chromium-docs/`).

## Configuration

The `data/configs/search_config.json` file controls search behavior and was manually created
based on Chromium's codebase structure. Key sections:

| Section | Purpose | When to Update |
|---------|---------|----------------|
| `indexing.scan_patterns` | Glob patterns for docs to index | New doc locations added |
| `indexing.excluded_patterns` | Directories to skip | New generated/vendor dirs |
| `categories` | Doc classification (api, testing, etc.) | Major component changes |
| `search.boost_factors` | Relevance weighting | Search quality tuning |

### Updating the Config

1. Edit `search_config.json` directly
2. Rebuild index: `python scripts/chromium_docs.py --build-index`
3. Test searches to verify results

Categories and patterns should match actual directory structures in the Chromium repo.

## Testing

### Run Unit Tests

```bash
cd agents/skills/chromium-docs/scripts
python -m pytest chromium_docs_test.py -v
# Or without pytest:
python chromium_docs_test.py
```

### Manual Verification

After making changes, verify the skill works correctly:

```bash
# 1. Build the index
python scripts/chromium_docs.py --build-index

# 2. Test search functionality
python scripts/chromium_docs.py "mojo ipc"
python scripts/chromium_docs.py "browser test"
python scripts/chromium_docs.py "gpu architecture"

# 3. Verify results include relevant docs with reasonable scores
```

**Expected behavior:**
- Search results should include relevant document titles and paths
- Results are ranked by relevance (title matches rank higher)
- Categories should match the document content
