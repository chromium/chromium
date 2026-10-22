---
name: context-hub/webui-lit-three-file-split
rubric: design
severity: should_fix
---

**Rule.** Structure each Context Hub WebUI `CrLitElement` component using the
three-file split (`<name>.ts` for the element class, `<name>.html.ts` for the
`getHtml` template, and `<name>.css` for styles) rather than defining inline
`html` templates or `css` styles inside `<name>.ts`.

**Why.** Separating logic, template markup, and styles into dedicated files
matches Chromium's Lit WebUI build pipeline (`html_to_wrapper` and
`css_to_wrapper`) and keeps component files modular and maintainable.

## Examples

Bad:

```ts
// topic_card.ts: inlining render() and styles directly in the .ts file.
import {CrLitElement, css, html} from '//resources/lit/v3_0/lit.rollup.js';

export class TopicCardElement extends CrLitElement {
  static override get styles() {
    return css`:host { display: block; }`;
  }

  override render() {
    return html`<div class="card"></div>`;
  }
}
```

Good:

```ts
// topic_card.ts: importing getCss and getHtml from companion files.
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_card.css.js';
import {getHtml} from './topic_card.html.js';

export class TopicCardElement extends CrLitElement {
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}
```
