#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium Documentation Search System

A unified module that provides document indexing and search capabilities
for Chromium's documentation ecosystem.
"""

import json
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


@dataclass
class SearchResult:
    """A single search result."""
    path: str
    title: str
    summary: str
    score: float
    category: str
    keywords: List[str]
    excerpt: str = ""


class ChromiumDocs:
    """Main interface for Chromium documentation search."""

    def __init__(self, src_root: str = None, config_path: str = None):
        """Initialize the documentation search system."""
        # Set default paths relative to skill location (scripts directory)
        skill_root = Path(__file__).parent.parent

        # Default to Chromium src root (parent of .claude directory)
        if src_root:
            self.src_root = Path(src_root)
        else:
            # ../../../ from skill root
            self.src_root = skill_root.parent.parent.parent

        self.data_dir = skill_root / "data"

        if config_path:
            self.config_path = Path(config_path)
        else:
            self.config_path = (self.data_dir / "configs" /
                                "search_config.json")

        self.config = self._load_config()
        self.doc_index = {}
        self.keyword_index = {}
        self.category_index = {}
        self._load_indexes()

    def _load_config(self) -> Dict:
        """Load search configuration."""
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except FileNotFoundError:
            return self._default_config()

    def _default_config(self) -> Dict:
        """Default configuration."""
        return {
            "indexing": {
                "max_file_size_mb": 10,
                "scan_patterns": ["docs/**/*.md", "*/README.md"],
                "excluded_patterns":
                ["third_party/", "out/", ".git/", ".claude/"]
            },
            "search": {
                "default_limit": 10,
                "max_limit": 50
            }
        }

    def _load_indexes(self):
        """Load existing indexes."""
        index_dir = self.data_dir / "indexes"

        try:
            with open(index_dir / "doc_index.json", 'r',
                      encoding='utf-8') as f:
                self.doc_index = json.load(f)
            with open(index_dir / "keyword_index.json", 'r',
                      encoding='utf-8') as f:
                self.keyword_index = json.load(f)
            with open(index_dir / "category_index.json", 'r',
                      encoding='utf-8') as f:
                self.category_index = json.load(f)
        except FileNotFoundError:
            # Indexes don't exist yet - will need to build them
            pass

    def build_index(self) -> Dict:
        """Build the documentation index."""
        print("Building Chromium documentation index...")

        # Scan for markdown files
        docs = self._scan_documents()
        print(f"Found {len(docs)} documentation files")

        # Process documents
        processed_docs = {}
        keyword_index = {}
        category_index = {}

        for i, doc_path in enumerate(docs):
            if i % 100 == 0:
                print(f"Processing {i}/{len(docs)}")

            parsed = self._parse_document(doc_path)
            if parsed:
                rel_path = str(doc_path.relative_to(self.src_root))
                processed_docs[rel_path] = parsed

                # Index keywords
                for keyword in parsed['keywords']:
                    if keyword not in keyword_index:
                        keyword_index[keyword] = []
                    keyword_index[keyword].append(rel_path)

                # Index by category
                category = parsed['category']
                if category not in category_index:
                    category_index[category] = []
                category_index[category].append(rel_path)

        # Save indexes
        self._save_indexes(processed_docs, keyword_index, category_index)

        return {
            'documents_processed': len(processed_docs),
            'total_keywords': len(keyword_index),
            'total_categories': len(category_index)
        }

    def search(self,
               query: str,
               category: Optional[str] = None,
               limit: int = 10) -> List[SearchResult]:
        """Search documentation."""
        if not self.doc_index:
            return []

        results = []
        query_terms = query.lower().split()

        for doc_path, doc_data in self.doc_index.items():
            # Skip if category filter doesn't match
            if category and doc_data.get('category') != category:
                continue

            score = self._calculate_score(doc_data, query_terms)
            if score > 0:
                results.append(
                    SearchResult(path=doc_path,
                                 title=doc_data.get('title', 'Untitled'),
                                 summary=doc_data.get('summary', ''),
                                 score=score,
                                 category=doc_data.get('category', 'general'),
                                 keywords=doc_data.get('keywords', []),
                                 excerpt=self._extract_excerpt(
                                     doc_data.get('content', ''),
                                     query_terms)))

        # Sort by score and limit results
        results.sort(key=lambda x: x.score, reverse=True)
        return results[:limit]

    def get_categories(self) -> Dict[str, int]:
        """Get available categories with document counts."""
        if not self.category_index:
            return {}

        return {cat: len(docs) for cat, docs in self.category_index.items()}

    def _scan_documents(self) -> List[Path]:
        """Scan for markdown documentation files."""
        docs = []
        patterns = self.config['indexing']['scan_patterns']
        excluded = self.config['indexing']['excluded_patterns']

        for pattern in patterns:
            for doc_path in self.src_root.glob(pattern):
                if doc_path.is_file() and doc_path.suffix == '.md':
                    # Check if excluded
                    rel_path = str(doc_path.relative_to(self.src_root))
                    if not any(ex in rel_path for ex in excluded):
                        docs.append(doc_path)

        return docs

    def _parse_document(self, doc_path: Path) -> Optional[Dict]:
        """Parse a markdown document."""
        try:
            with open(doc_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            if not content.strip():
                return None

            # Extract basic information
            title = self._extract_title(content)
            summary = self._extract_summary(content)
            keywords = self._extract_keywords(content)
            category = self._categorize_document(doc_path, content)

            return {
                'title': title,
                'summary': summary,
                'content': content,
                'keywords': keywords,
                'category': category,
                'mtime': doc_path.stat().st_mtime
            }

        except Exception as e:
            print(f"Error parsing {doc_path}: {e}")
            return None

    def _extract_title(self, content: str) -> str:
        """Extract document title."""
        lines = content.split('\n')

        # Look for H1 heading
        for line in lines:
            line = line.strip()
            if line.startswith('# '):
                return line[2:].strip()

        # Look for H2 heading if no H1
        for line in lines:
            line = line.strip()
            if line.startswith('## '):
                return line[3:].strip()

        return "Untitled Document"

    def _extract_summary(self, content: str) -> str:
        """Extract document summary."""
        lines = content.split('\n')
        summary_lines = []

        for line in lines:
            line = line.strip()
            # Skip headers and short lines
            if not line.startswith('#') and len(line) > 30:
                summary_lines.append(line)
                if len(' '.join(summary_lines)) > 200:
                    break

        summary = ' '.join(summary_lines)
        return summary[:300] + "..." if len(summary) > 300 else summary

    def _extract_keywords(self, content: str) -> List[str]:
        """Extract keywords from content."""
        chromium_terms = {
            # Core processes and architecture
            'browser',
            'renderer',
            'gpu',
            'utility',
            'network',
            'service',
            # Communication and IPC
            'mojo',
            'ipc',
            'pipe',
            'message',
            'interface',
            'binding',
            # Web technologies
            'blink',
            'v8',
            'webrtc',
            'webgl',
            'javascript',
            'html',
            'css',
            # Security
            'sandbox',
            'site-isolation',
            'permission',
            'origin',
            'cors',
            # Platforms
            'chromeos',
            'android',
            'ios',
            'windows',
            'linux',
            'mac',
            # Development tools
            'gn',
            'ninja',
            'gclient',
            'depot-tools',
            'unittest',
            'browsertest',
            # Graphics and media
            'skia',
            'vulkan',
            'opengl',
            'webassembly',
            'codec',
            # Core APIs
            'api',
            'mojom',
            'content',
            'chrome',
            'chromium'
        }

        keywords = set()
        content_lower = content.lower()

        # Find Chromium-specific terms
        for term in chromium_terms:
            if term in content_lower:
                keywords.add(term)

        # Extract technical identifiers
        camel_matches = re.findall(r'\b[A-Z][a-z]+(?:[A-Z][a-z]+)+\b', content)
        for match in camel_matches:
            if len(match) > 4:
                keywords.add(match.lower())

        return sorted(list(keywords))[:20]

    def _categorize_document(self, doc_path: Path, content: str) -> str:
        """Categorize document based on path and content."""
        path_str = str(doc_path).lower()
        content_lower = content.lower()

        # Path-based categorization (most specific first)
        if ('test' in path_str or 'testing/' in path_str
                or 'unittest' in content_lower
                or 'browser_test' in content_lower):
            return 'testing'
        elif ('gpu/' in path_str or 'graphics/' in path_str
              or 'webgl' in content_lower or 'vulkan' in content_lower):
            return 'gpu'
        elif ('security/' in path_str or 'sandbox/' in path_str
              or 'site-isolation' in content_lower
              or 'permission' in content_lower):
            return 'security'
        elif ('net/' in path_str or 'network/' in path_str
              or 'http' in content_lower or 'quic' in content_lower):
            return 'network'
        elif ('ui/' in path_str or 'views/' in path_str
              or 'aura/' in path_str):
            return 'ui'
        elif ('build/' in path_str or 'gn/' in path_str
              or 'ninja' in content_lower or 'compilation' in content_lower):
            return 'build'
        elif ('media/' in path_str or 'audio/' in path_str
              or 'video/' in path_str):
            return 'media'
        elif 'android/' in path_str or 'java/' in path_str:
            return 'android'
        elif 'ios/' in path_str:
            return 'ios'
        elif 'chromeos/' in path_str or 'ash/' in path_str:
            return 'chromeos'
        elif (('api' in content_lower and 'interface' in content_lower)
              or 'mojom' in content_lower):
            return 'api'
        elif ('architecture' in content_lower or 'design-document' in path_str
              or 'multi-process' in content_lower):
            return 'architecture'
        elif ('performance' in content_lower or 'benchmark' in content_lower
              or ('memory' in content_lower and 'usage' in content_lower)):
            return 'performance'
        elif 'accessibility' in path_str or 'a11y' in content_lower:
            return 'accessibility'
        elif ('dev' in path_str or 'debug' in content_lower
              or 'tools/' in path_str):
            return 'development'

        return 'general'

    def _calculate_score(self, doc_data: Dict,
                         query_terms: List[str]) -> float:
        """Calculate relevance score with improved matching."""
        score = 0.0
        title = doc_data.get('title', '').lower()
        content = doc_data.get('content', '').lower()
        keywords = [k.lower() for k in doc_data.get('keywords', [])]
        file_path = (doc_data.get('path', '').lower()
                     if 'path' in doc_data else '')

        for term in query_terms:
            term_lower = term.lower()

            # Exact title matches are highest priority
            if term_lower in title:
                score += 4.0

            # Path matches (for component-specific searches)
            if file_path and term_lower in file_path:
                score += 2.5

            # Keyword matches
            if term_lower in keywords:
                score += 2.0

            # Exact word boundaries in content (more precise)
            if f' {term_lower} ' in f' {content} ':
                score += 1.5
            elif term_lower in content:
                score += 1.0

            # Partial matches for compound terms
            for keyword in keywords:
                if term_lower in keyword or keyword in term_lower:
                    score += 0.5

        # Boost recent documents slightly
        doc_age_days = (time.time() - doc_data.get('mtime', 0)) / 86400
        if doc_age_days < 30:  # Less than 30 days old
            score *= 1.1

        return score

    def _extract_excerpt(self, content: str, query_terms: List[str]) -> str:
        """Extract relevant excerpt from content."""
        lines = content.split('\n')

        for line in lines:
            line_lower = line.lower()
            if any(term in line_lower for term in query_terms):
                excerpt = line.strip()
                return excerpt[:150] + "..." if len(line) > 150 else excerpt

        return ""

    def _save_indexes(self, docs: Dict, keywords: Dict, categories: Dict):
        """Save indexes to disk."""
        index_dir = self.data_dir / "indexes"
        index_dir.mkdir(exist_ok=True, parents=True)

        with open(index_dir / "doc_index.json", 'w', encoding='utf-8') as f:
            json.dump(docs, f, indent=2)

        with open(index_dir / "keyword_index.json", 'w',
                  encoding='utf-8') as f:
            json.dump(keywords, f, indent=2)

        with open(index_dir / "category_index.json", 'w',
                  encoding='utf-8') as f:
            json.dump(categories, f, indent=2)

        self.doc_index = docs
        self.keyword_index = keywords
        self.category_index = categories


# Main API functions for SKILL
def search_chromium_docs(query: str, category: str = None) -> str:
    """Main search function called by AI assistant."""
    docs = ChromiumDocs()

    if not docs.doc_index:
        return """**Chromium Documentation Search Not Available**

The documentation index needs to be built first. Run:
```bash
cd agents/skills/chromium-docs
python scripts/chromium_docs.py --build-index
```"""

    results = docs.search(query, category)

    if not results:
        return f"No documentation found for '{query}'"

    output = [f"**Found {len(results)} Chromium documentation results:**\n"]

    for i, result in enumerate(results[:10], 1):
        link = f"[{result.title}]({result.path})"
        output.append(f"**{i}. {link}**")
        output.append(f"   📂 *{result.category.title()}*")
        if result.excerpt:
            output.append(f"   💡 {result.excerpt}")
        elif result.summary:
            summary = (result.summary[:120] +
                       "..." if len(result.summary) > 120 else result.summary)
            output.append(f"   📄 {summary}")
        output.append("")

    return "\n".join(output)


def show_doc_categories() -> str:
    """Show available documentation categories."""
    docs = ChromiumDocs()
    categories = docs.get_categories()

    if not categories:
        return "No documentation categories available. Build the index first."

    output = ["**Available Chromium Documentation Categories:**\n"]
    for category, count in sorted(categories.items(),
                                  key=lambda x: x[1],
                                  reverse=True):
        output.append(f"• **{category.title()}** ({count} documents)")

    return "\n".join(output)


def browse_docs_category(category: str) -> str:
    """Browse documentation by category."""
    docs = ChromiumDocs()
    results = docs.search("", category, limit=15)

    if not results:
        return f"No documents found in category '{category}'"

    output = [
        f"**{category.title()} Documentation ({len(results)} documents):**\n"
    ]

    for i, result in enumerate(results, 1):
        link = f"[{result.title}]({result.path})"
        output.append(f"{i}. {link}")
        if i <= 5 and result.summary:
            summary = (result.summary[:100] +
                       "..." if len(result.summary) > 100 else result.summary)
            output.append(f"   {summary}\n")

    return "\n".join(output)


if __name__ == "__main__":
    chromium_docs = ChromiumDocs()

    if len(sys.argv) > 1:
        if sys.argv[1] == "--build-index":
            build_result = chromium_docs.build_index()
            print(f"Index built: {build_result}")
        else:
            search_query = " ".join(sys.argv[1:])
            search_result = search_chromium_docs(search_query)
            print(search_result)
    else:
        categories_result = show_doc_categories()
        print(categories_result)
