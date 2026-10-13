---
name: actor-tool-registration
description: >-
  Use when asked to "register a tool in the tool registry", "add a ToolId",
  "add GetToolDefinition", "expose an actor tool schema", or when working on a
  "Register XToolRequest in ToolRegistry" task. Don't use for implementing a
  brand-new Actor tool from scratch or for migrating ToolController callers to
  the registry.
---

# Registering an Actor Tool in `actor::ToolRegistry`

Each registration task gives an existing `ToolRequest` subclass in
`chrome/browser/actor/tools/` a model-facing schema. A registration CL touches
**strictly 4 files** (never touches `tool_registry.{h,cc}`, `BUILD.gn`, or
request unittests):

1. `chrome/browser/actor/tools/registry/tool_definition.h`
2. `chrome/browser/actor/tools/<stem>_tool_request.h`
3. `chrome/browser/actor/tools/<stem>_tool_request.cc`
4. `chrome/browser/actor/tools/registry/tool_registry_unittest.cc`

Follow the 1:1 `ToolRequest` → `ToolDefinition` exemplar in
[crrev.com/c/8477400](https://crrev.com/c/8477400) (`NavigateToolRequest` in
`chrome/browser/actor/tools/navigate_tool_request.{h,cc}`).

## 1. Edit the 4 Files

### `chrome/browser/actor/tools/registry/tool_definition.h`

- Append `k<Name> = <next_int>,` at the end of `enum class ToolId` (just before
  `kMaxValue`) with a one-line comment, and update `kMaxValue = k<Name>,`.
  `ToolId` is append-only; never renumber existing entries.

### `chrome/browser/actor/tools/<stem>_tool_request.h`

- Below `static constexpr char kName[]`, declare `kModelFacingName` and
  parameter keys (omit param constants for no-arg tools):
  ```cpp
  static constexpr std::string_view kModelFacingName = "<snake_case_name>";
  // JSON argument key for ...
  static constexpr std::string_view k<Param>Param = "<name>";
  ```
- Declare the static getter directly after the destructor:
  ```cpp
  // Returns the `ToolId::k<Name>` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();
  ```
- `#include <optional>` and `#include <string_view>`.

### `chrome/browser/actor/tools/<stem>_tool_request.cc`

- Include `<optional>`, `<string_view>`,
  `"chrome/browser/actor/tools/registry/tool_definition.h"`, and
  `"chrome/browser/actor/tools/registry/tool_definition_builder.h"`. (Include
  `"tool_schema_builder.h"` if referencing format constants or building
  non-empty schemas).
- Define `k<Name>ToolDescription` and `k<Param>ParamDescription` in the
  anonymous namespace. Match existing descriptions in
  `chrome/browser/ttc/core/tool_controller.cc` or `tools.mojom` verbatim.
- Build and return the definition:
  ```cpp
  // static
  std::optional<ToolDefinition> NavigateToolRequest::GetToolDefinition() {
    return ToolDefinitionBuilder(ToolId::kNavigate, kModelFacingName,
                                 kNavigateToolDescription)
        .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
            kUrlParam, kUrlParamDescription, ToolSchemaBuilder::kFormatUri))
        .Build();
  }
  ```
- **No-arg tools**: omit `.SetToolParameterSchema(...)`; `Build()` defaults to
  an empty object schema.
- **Required parameters**: `ToolSchemaBuilder` marks all properties required (no
  optional parameter support). If a tool has optional modes, split it into
  separate 1:1 `ToolRequest` subclasses.
- **Supported property builders**: `.AddStringProperty(name, desc, format)`,
  `.AddIntegerProperty(name, desc)`,
  `.AddStringEnumProperty(key, values, desc)`. Enum values use a plain C-array
  (`constexpr std::string_view kValues[] = {"a", "b"};`).
- **Missing a property type?** (e.g. boolean, list): Add
  `ToolSchemaBuilder& Add<Type>Property(...)` in a separate parent CL in
  `chrome/browser/actor/tools/registry/tool_schema_builder.{h,cc}` routing
  through `AddPropertyImpl()`, covered in `tool_schema_builder_unittest.cc`:
  ```cpp
  ToolSchemaBuilder& ToolSchemaBuilder::AddBooleanProperty(
      std::string_view name, std::string_view description) {
    base::DictValue prop_schema;
    prop_schema.Set("type", "boolean");
    prop_schema.Set("description", description);
    AddPropertyImpl(name, std::move(prop_schema));
    return *this;
  }
  ```

### `chrome/browser/actor/tools/registry/tool_registry_unittest.cc`

All schema tests live here. Add the `#include` alphabetically and define two
tests (replace `Navigate` and `kUrlParam` with your tool and parameter keys):

```cpp
TEST(ToolRegistryTest, NavigateToolDefinition) {
  const std::optional<ToolDefinition> definition =
      NavigateToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kNavigate);
  EXPECT_EQ(definition->name, NavigateToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(NavigateToolRequest::kUrlParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(NavigateToolRequest::kUrlParam));
}

TEST(ToolRegistryTest, GetAllToolsContainsNavigateTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kNavigate, &ToolDefinition::id));
}
```

- **No-arg tools**: replace `HasParamOfType`/`RequiresParam` with:
  ```cpp
  EXPECT_THAT(
      definition->parameters_json_schema,
      base::test::DictionaryHasValue("properties", base::DictValue()));
  ```
  (and include `"base/test/values_test_util.h"` and `"base/values.h"`).
- **Desktop-only tools**: If the tool header is guarded by
  `#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)` in
  `chrome/browser/actor/tool_request_variant.h` (e.g.
  `switch_tab_tool_request.h`), wrap its include and tests with the same guard
  and `#include "chrome/browser/actor/actor_buildflags.h"`.
- Matchers `HasParamOfType` and `RequiresParam` live in
  `//chrome/browser/actor/tools/registry:test_support`. Do not add redundant
  tests for `NameToToolId` or uniqueness (`ToolRegistryCompletenessTest.*`
  already covers them).
- No test-only hooks in production code.

## 2. Verify & Rebase

```bash
tools/autotest.py -C out/Default \
  chrome/browser/actor/tools/registry/tool_registry_unittest.cc
gn check out/Default //chrome/browser/actor/tools/registry/* //chrome/browser/actor/*
git cl format
git status --short
```

Verify `git status --short` shows **strictly 4 `M` lines and zero `??` lines**.

- **Merge conflicts**: Registration CLs frequently conflict on
  `enum class ToolId` and includes/tests in `tool_registry_unittest.cc`. Rebase
  and assign the next sequential integer to `ToolId`, keeping other additions.
