# cr-tab-box

cr-tab-box is a non-Polymer custom element that can be used to create a simple
tabbed UI. This is generally most useful for debug pages that are not
concerned with matching the look/feel of the larger user-facing Chromium WebUIs
that use Polymer (e.g. chrome://settings). cr-tab-box replaces the deprecated
cr.ui.TabBox from ui/webui/resources/js/tabs.js which was previously
used by debug pages for this purpose.

## Example usage
Tabs and tab panels can be added into the appropriate slots. The number and
order of the tabs should match the number and order of the panels. Example:

```html
  <cr-tab-box>
    <div slot="tab">Donuts</div>
    <div slot="tab">Cookies</div>
    <div slot="panel">
      <span>Some content related to donuts</span>
    </div>
    <div slot="panel">
      <span>Some content related to cookies</span>
    </div>
  </cr-tab-box>
```
## Relationship to cr-tabs
In general, user facing WebUIs using Polymer should use cr-tabs, while debug
UIs trying to avoid Polymer (e.g., in order to run on mobile platforms) should
use cr-tab-box. Key differences include:

* cr-tab-box contains slots for both tabs and corresponding panels. cr-tabs
  has only one slot for tabs, and is generally used in combination with
  something like iron-pages.
* cr-tabs is kept up to date with current user-facing WebUI styles. cr-tab-box
  is intended for debug UIs and uses very simple styling.
* cr-tabs depends on Polymer, while cr-tab-box does not.

