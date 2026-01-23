// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// A utility class for classifying images in Readability distilled content.
// Uses a prioritized cascade of heuristics to classify an image as either
// inline (e.g., icon) or full-width (e.g., feature image). The checks are:
// 1. Rendered size vs. viewport size (for visually dominant images).
// 2. Intrinsic size and metadata (for small or decorative images).
// 3. Structural context (e.g., inside a <figure>).
// 4. A final fallback based on intrinsic width.
// All checks use density-independent units (CSS pixels).
export class ReadabilityImageClassifier {
  static readonly INLINE_CLASS = 'distilled-inline-img';
  static readonly FULL_WIDTH_CLASS = 'distilled-full-width-img';
  static readonly DOMINANT_IMAGE_MIN_VIEWPORT_RATIO = 0.8;
  static readonly SRC_CANDIDATES = [
    // Used by lazysizes and UI Frameworks like Bootstrap.
    'data-src',
    // Mostly used in websites that depend on jQuery LazyLoad.
    'data-original',
    // WordPress standard, injected by plugins like WP Rocket or Smush.
    'data-lazy-src',
    // Other implementations.
    'data-url',
    'data-image',
  ];

  static readonly SRCSET_CANDIDATES = [
    'data-srcset',
    'data-lazy-srcset',
    'data-original-set',
  ];

  static readonly SIZES_CANDIDATES = [
    'data-sizes',
    'data-lazy-sizes',
  ];

  private smallAreaUpperBoundDp: number;
  private inlineWidthFallbackUpperBoundDp: number;
  private mathyKeywordsRegex_: RegExp;
  private mathyAltTextRegex_: RegExp;
  private filenameRegex_: RegExp;

  constructor() {
    // Baseline thresholds in density-independent units (CSS pixels).
    this.smallAreaUpperBoundDp = 64 * 64;
    this.inlineWidthFallbackUpperBoundDp = 300;

    // Matches common keywords for icons or mathematical formulas.
    const mathyKeywords =
        ['math', 'latex', 'equation', 'formula', 'tex', 'icon'];
    this.mathyKeywordsRegex_ =
        new RegExp('\\b(' + mathyKeywords.join('|') + ')\\b', 'i');

    // Matches characters commonly found in inline formulas.
    this.mathyAltTextRegex_ = /[+\-=_^{}\\]/;

    // Extracts the filename from a URL path.
    this.filenameRegex_ = /(?:.*\/)?([^?#]*)/;
  }

  // Checks for strong signals that the image is INLINE based on its intrinsic
  // properties.
  private isDefinitelyInline_(img: HTMLImageElement): boolean {
    const widthAttr = parseInt(img.getAttribute('width') || '0', 10);
    const heightAttr = parseInt(img.getAttribute('height') || '0', 10);

    // If explicit dimensions are defined and small (e.g., < 100px), it's
    // inline.
    if (widthAttr > 0 && widthAttr < 100 && heightAttr > 0 &&
        heightAttr < 100) {
      return true;
    }

    // Use natural dimensions (in CSS pixels) to check for small area.
    const area = img.naturalWidth * img.naturalHeight;
    if (area > 0 && area < this.smallAreaUpperBoundDp) {
      return true;
    }

    // "Mathy" or decorative clues in attributes.
    const classAndId = (img.className + ' ' + img.id);
    if (this.mathyKeywordsRegex_.test(classAndId)) {
      return true;
    }

    // Check the filename of the src URL, ignoring data URIs.
    if (img.src && !img.src.startsWith('data:')) {
      const filename = img.src.match(this.filenameRegex_)?.[1] || '';
      if (filename && this.mathyKeywordsRegex_.test(filename)) {
        return true;
      }
    }

    // "Mathy" alt text.
    const alt = img.getAttribute('alt') || '';
    if (alt.length > 0 && alt.length < 80 &&
        this.mathyAltTextRegex_.test(alt)) {
      return true;
    }

    return false;
  }

  // Checks if the image is the primary content of its container.
  private isDefinitelyFullWidth_(img: HTMLImageElement): boolean {
    // Image is in a <figure> with a <figcaption>.
    const parent = img.parentElement;
    if (parent && parent.tagName === 'FIGURE' &&
        parent.querySelector('figcaption')) {
      return true;
    }

    // Image is the only significant content in its container.
    let container: HTMLElement|null = parent;
    while (container &&
           !['P', 'DIV', 'FIGURE', 'BODY'].includes(container.tagName)) {
      container = container.parentElement;
    }

    if (container) {
      for (const child of Array.from(container.childNodes)) {
        // Skip insignificant nodes.
        if (child === img) {
          continue;
        }
        if ((child as HTMLElement).tagName === 'BR') {
          continue;
        }
        if (child.nodeType === Node.TEXT_NODE &&
            child.textContent?.trim() === '') {
          continue;
        }

        // If we reach this point, the node must be significant.
        return false;
      }
      // If we finish the loop, no significant siblings were found.
      return true;
    }

    return false;
  }

  // Classifies the image based on a simple intrinsic width fallback.
  private classifyByFallback_(img: HTMLImageElement): string {
    // Use naturalWidth (in CSS pixels) and compare against the dp threshold.
    return img.naturalWidth > this.inlineWidthFallbackUpperBoundDp ?
        ReadabilityImageClassifier.FULL_WIDTH_CLASS :
        ReadabilityImageClassifier.INLINE_CLASS;
  }

  // Detects lazy-loading attributes and moves them to standard attributes
  // (src, srcset, sizes) to trigger native loading.
  // This is necessary for static environments where the original page's
  // lazy-loading JavaScript does not run, ensuring the real content is loaded
  // instead of a placeholder.
  private loadLazyImageAttributes_(img: HTMLImageElement): void {
    if (!img.src || img.src.startsWith('data:')) {
      const srcAttribute = ReadabilityImageClassifier.SRC_CANDIDATES.find(
          el => img.hasAttribute(el));
      if (srcAttribute) {
        const val = img.getAttribute(srcAttribute);
        if (val) {
          img.src = val;
          img.removeAttribute(srcAttribute);
        }
      }
    }

    const srcsetAttribute = ReadabilityImageClassifier.SRCSET_CANDIDATES.find(
        el => img.hasAttribute(el));
    if (srcsetAttribute) {
      const val = img.getAttribute(srcsetAttribute);
      if (val) {
        img.srcset = val;
        img.removeAttribute(srcsetAttribute);
      }
    }

    const sizeAttribute = ReadabilityImageClassifier.SIZES_CANDIDATES.find(
        el => img.hasAttribute(el));
    if (sizeAttribute) {
      const val = img.getAttribute(sizeAttribute);
      if (val) {
        img.sizes = val;
        img.removeAttribute(sizeAttribute);
      }
    }
  }

  // Determines an image's display style using a prioritized cascade of checks.
  classify(img: HTMLImageElement): string {
    // Check for visually dominant images first, as this is the most reliable
    // signal and overrides all other heuristics.
    const renderedWidth = img.getBoundingClientRect().width;
    if (renderedWidth > 0 && window.innerWidth > 0 &&
        (renderedWidth / window.innerWidth) >
            ReadabilityImageClassifier.DOMINANT_IMAGE_MIN_VIEWPORT_RATIO) {
      return ReadabilityImageClassifier.FULL_WIDTH_CLASS;
    }

    // Fall back to checks based on intrinsic properties and structure.
    if (this.isDefinitelyInline_(img)) {
      return ReadabilityImageClassifier.INLINE_CLASS;
    }

    if (this.isDefinitelyFullWidth_(img)) {
      return ReadabilityImageClassifier.FULL_WIDTH_CLASS;
    }

    return this.classifyByFallback_(img);
  }

  // Post-processes all images in an element to apply classification classes.
  static processImagesIn(element: HTMLElement): void {
    const classifier = new ReadabilityImageClassifier();
    const images = element.getElementsByTagName('img');

    const applyClassification = (img: HTMLImageElement) => {
      const classification = classifier.classify(img);
      img.classList.add(classification);
    };

    // Type definition for the handler
    const imageLoadHandler = (event: Event) => {
      const img = event.currentTarget as HTMLImageElement;
      applyClassification(img);
    };

    for (let i = 0; i < images.length; i++) {
      const img = images[i];
      if (!img) {
        continue;
      }

      classifier.loadLazyImageAttributes_(img);
      // If the image is already loaded (e.g., from cache), manually trigger.
      if (img.complete) {
        applyClassification(img);
      } else {
        img.onload = imageLoadHandler;
      }
    }
  }
}
