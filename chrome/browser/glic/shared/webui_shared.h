#ifndef CHROME_BROWSER_GLIC_SHARED_WEBUI_SHARED_H_
#define CHROME_BROWSER_GLIC_SHARED_WEBUI_SHARED_H_

// Functionality shared between the glic and glic-fre WebUIs.

namespace content {
class WebUIDataSource;
}  // namespace content

namespace glic {

// Configures the given WebUIDataSource with values shared between the glic and
// glic-fre WebUIs.
void ConfigureSharedWebUISource(content::WebUIDataSource& source);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SHARED_WEBUI_SHARED_H_
