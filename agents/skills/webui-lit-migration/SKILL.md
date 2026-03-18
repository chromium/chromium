---
name: webui-lit-migration
description: Guide for migrating Chromium WebUI components from Polymer to Lit.
Activate this skill if the user asks to migrate a file or component to Lit.
---

# Chromium WebUI Lit Migration

## Step 1: Run scripts/run_codemod_script.sh

IMPORTANT: Always do this first, as it automates trivial migration steps.

Run the script in this skill's scripts directory. Pass the path to the Polymer
based element's TS class definition file as the parameter. Example:
./webui-lit-migration/scripts/run_codemod_script.sh \
chrome/browser/resources/certificate_manager/certificate_entry.ts

## Step 2: .ts file cleanup

1. Replace PolymerElement import The script replaces this import except in cases
   where multiple things are imported from Polymer. If an import from
   polymer_bundled.min.js is still present after running the script, remove it
   and add: import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

1. Update 'extends PolymerElement' The script will update this in the case that
   no mixins are used. If there is still a reference to PolymerElement: replace
   SomeMixin(PolymerElement) with SomeMixinLit(CrLitElement)

1. Address any "TODO: Port this observer to Lit" comments left by the migration
   script as follows: a. Examine the observer (e.g. 'onFooChanged\_') method. If
   it does not reference the DOM, add the following in a willUpdate callback. If
   it does reference the DOM (e.g., this.shadowRoot or this.$), add this in a
   updated() lifecycle callback instead.

```ts
if (changedProperties.has('foo')) {
  this.onFooChanged_();
}
```

```
b. If the property being observed is protected or private, the
   changedProperties lifecycle method parameter will require a cast:
```

```ts
const changedPrivateProperties = changedProperties as Map<PropertyKey, unknown>;
if (changedPrivateProperties.has('myProperty_')) { ... }
```

```
c. Remove the added TODO and the commented out observer line.
```

4. Address any complex observers in a similar way. Look for an "observers: "
   line:

```ts
observers: ['onFooOrBarChanged_(foo, bar)']
```

Remove the line and for each observer listed, add to a lifecycle method as
described for single property observers, but check changedProperties for any of
the properties listed in the observer:

```
if (changedProperties.has('foo') || changedProperties.has('bar')) {
  this.onFooOrBarChanged_();
}
```

5. Move value initialization out of 'properties' to the declaration.

Replace

```
  foo: {
    type: String,
    value: 'foo',
  },
  ...
  value: string;
```

with

```
  foo: {
    type: String,
  },
  ...
  accessor value: string = 'foo';
```

For uninitialized properties, if a TS compiler error is thrown on build,
initialize to a dummy or default value rather than using a non-null assertion
operator or changing the type definition.

## Step 3: Check for any missing shared CSS styles.

If any imported style file does not exist, check if the Polymer version (same
file path, but without \_lit suffix) exists. If so, generate the Lit version as
follows:

1. **Create Lit Version**: Create `[shared_style]_style_lit.css`.
1. **Copy Content**: Copy the CSS rules to the new `*_lit.css` file.
1. **Update Metadata**: Use `#type=style-lit` in the metadata. Update any
   imported and included styles to the lit version (e.g. cr_shared_style.css.js
   import --> cr_shared_style_lit.css.js import, include="cr-shared-style"
   becomes include="cr-shared-style-lit").
1. **Empty Original**: Clear the original `*_style.css` file, leaving only its\]
   metadata header and a comment:
   ```css
   /* Purposefully empty since this style is generated at build time from the
    * equivalent Lit version. */
   ```
1. **Import in Component**: Component CSS should import the new `*_lit.css.js`
   and include it.
1. **BUILD.gn**: Add the new `*_lit.css` to `css_files`.

## Step 4: Update .html.ts template file

1. Replace any <template is="dom-if" if=[[someCondition]]> with conditional
   rendering.

```
${this.someCondition ? html`<conditionally-rendered-element>` : ''}
```

2. Replace any <template is="dom-repeat" items=[[myItems]]> with map():

```
  ${this.myItems.map((item, index) => html`<some-item data="${item}"></some-item>`)
```

3. Look for on-foo-changed event listeners that were added by the script to
   replace Polymer double bindings. Ex:

```
<cr-input .value="${this.value_}" @value-changed="${this.onValueChanged_}">
```

Add a corresponding onFooChanged() method to the .ts file that updates the bound
property from event.detail.value:

```
protected onFooChanged(e: CustomEvent<{value: string}>) {
  this.value_ = e.detail.value;
}
```

4. Update attribute bindings. Polymer uses "attr-name$=" syntax for attribute
   bindings. Replace this with "?attr-name=" if the attribute is a boolean, or
   "attr-name=" if the attribute is a string/number.

5. Update property bindings. Any other bindings that were bound using
   "property-name=" syntax should migrate to Lit's property syntax:
   ".propertyName=".

6. Look for properties that are passed to methods in the HTML. If the method is
   used in multiple places in the template with different parameters or is not a
   class method, keep the properties as parameters and add `this.` to reference
   them. Ex:

Replace:

```
<div aria-label="${this.i18n('foo', someProp)}"></div>
<button>${this.i18n('buttonLabel', somOtherProp)}</button>
```

with:

```
<div aria-label="${this.i18n('foo', this.someProp)}"></div>
<button>${this.i18n('buttonLabel', this.somOtherProp)}</button>
```

If a method is a class member method used in a single location, remove the
property parameters in the template and change the method to reference them
directly. Ex: if `getDivClass()` is only used in this location, then:

Replace:

```
<div class="${this.getDivClass(someProp, someOtherProp)}">
```

```
protected getDivClass(value: string, otherValue: string) {
  return value + ' ' + otherValue;
}
```

with

```
<div class="${this.getDivClass()}">
```

```
protected getDivClass() {
  return this.someProp + ' ' + this.someOtherProp;
}
```

7. Identify any enums that are reactive properties for the purposes of using
   them from the template, and replace by importing the enum directly in the
   .html.ts file. Then clean up the enum reactive property in the .ts file.

## Step 5: BUILD.gn file updates

Find the BUILD.gn file referencing the old .html and .ts files. It should be in
the same directory as the .ts file or in an ancestor directory. If you can't
locate it, prompt the user for the BUILD.gn file to update.

1. **Web Component to TS**: Move the component's `.ts` file from
   `web_component_files` to `ts_files`.
2. **Add HTML Template**: Add the new `[component].html.ts` file to `ts_files`.
3. **Update CSS**: Add the component's `.css` and any new shared style
   `*_lit.css` files to `css_files`.
4. **Platform Specifics**: For ChromeOS-only or other platform-specific
   components/files, ensure they are added within the appropriate conditional
   blocks (e.g., `if (is_chromeos) { ... }`).

## Step 6: Validation

1. Build resources target to check for TS compiler or eslint errors.
   <out_folder> is the desired build output directory, commonly out/Default or
   similar. Prompt the user to identify this directory if it cannot be
   identified automatically.

```
autoninja -C <out_folder> <migrating_directory>:resources
```

Address any errors. Note that error line numbers for TSC and eslint correspond
to preprocessed code, not source code. Look at the line number, open the
generated preprocessed file, which is at
\<build_dir>/gen/\<migrating_directory>/preprocessed, look at the error line,
and then find this line in the corresponding source file to fix it.

2. Build tests and manual checking targets:

```
autoninja -C <out_folder> chrome browser_tests interactive_ui_tests
```

\*\* Important: Always re-build the full test target when debugging. \*\*
Resources are served from .pak files that will not be updated properly without
performing a full build.

3. Identify the test directory for the migrating WebUI. It should be located at
   chrome/test/data/webui/\<path_from_chrome_browser_resources>/. E.g., tests
   for chrome/browser/resources/certificate_manager are at
   chrome/test/data/webui/certificate_manager. If no such directory is found,
   prompt the user to ask for the WebUI test directory.

4. Examine any .cc files found in the test directory to find the names of the
   TEST_F targets to gtest_filter for. By convention, .cc files ending in
   browsertest.cc or browser_tests.cc or similar will be run as part of the
   browser_tests target. .cc files ending in focus_test.cc or
   interactive_test.cc will be run as part of the interactive_ui_tests target.

5. Run *all* tests found in all .cc files in the WebUI's test directory and
   debug any test failures.

```
<out_folder>/browser_tests --gtest_filter=<TestNamePatternHere>
<out_folder>/interactive_ui_tests --gtest_filter=<TestNamePatternHere>
```

6. Run git cl format --js on the prod and test directories and run presubmits.
