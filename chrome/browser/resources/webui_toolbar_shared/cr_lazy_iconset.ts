// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Iconset} from '//resources/cr_elements/cr_icon/iconset_map.js';
import {IconsetMap} from '//resources/cr_elements/cr_icon/iconset_map.js';
import {assert} from '//resources/js/assert.js';

const APPLIED_ICON_CLASS = 'cr-iconset-svg-icon_';

export class CrLazyIconset implements Iconset {
  readonly name: string;
  readonly size: number;
  private parsedIcons_: Map<string, SVGGElement> = new Map();
  private lazyIcons_: Map<string, () => (TrustedHTML | string)> = new Map();

  constructor(name: string, size: number = 24) {
    this.name = name;
    this.size = size;
    IconsetMap.getInstance().set(this.name, this);
  }

  registerIcons(iconsMap: Record<string, () => (TrustedHTML | string)>) {
    for (const [id, callback] of Object.entries(iconsMap)) {
      assert(!this.lazyIcons_.has(id));
      this.lazyIcons_.set(id, callback);
    }
  }

  applyIcon(element: HTMLElement, iconName: string): SVGElement|null {
    this.removeIcon(element);
    const svg = this.cloneIcon_(iconName);
    if (svg) {
      svg.classList.add(APPLIED_ICON_CLASS);
      element.shadowRoot!.insertBefore(svg, element.shadowRoot!.childNodes[0]!);
      return svg;
    }
    return null;
  }

  createIcon(iconName: string): SVGElement|null {
    return this.cloneIcon_(iconName);
  }

  removeIcon(element: HTMLElement) {
    const oldSvg =
        element.shadowRoot!.querySelector<SVGElement>(`.${APPLIED_ICON_CLASS}`);
    if (oldSvg) {
      oldSvg.remove();
    }
  }

  private cloneIcon_(id: string): SVGElement|null {
    let sourceSvg: SVGGElement|null = null;
    if (this.parsedIcons_.has(id)) {
      sourceSvg = this.parsedIcons_.get(id)!;
    } else if (this.lazyIcons_.has(id)) {
      const callback = this.lazyIcons_.get(id);
      assert(callback);
      const tempDiv = document.createElement('div');
      tempDiv.innerHTML = callback();
      const svgElement = tempDiv.firstElementChild;
      assert(svgElement);
      sourceSvg = svgElement.firstElementChild as SVGGElement;
      this.parsedIcons_.set(id, sourceSvg);
      this.lazyIcons_.delete(id);
    }

    assert(sourceSvg);

    const svgClone =
        document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const content = sourceSvg.cloneNode(true) as SVGGElement;
    content.removeAttribute('id');
    const contentViewBox = content.getAttribute('viewBox');
    if (contentViewBox) {
      svgClone.setAttribute('viewBox', contentViewBox);
    }
    svgClone.style.display = 'block';
    svgClone.style.height = '100%';
    svgClone.style.width = '100%';
    svgClone.style.pointerEvents = 'none';
    svgClone.appendChild(content);
    return svgClone;
  }
}
