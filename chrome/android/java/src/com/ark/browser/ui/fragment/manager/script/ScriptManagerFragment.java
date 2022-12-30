package com.ark.browser.ui.fragment.manager.script;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.toast.ZToast;
import com.zpj.fragmentation.dialog.impl.AttachListDialogFragment;
import com.zpj.utils.ClickHelper;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.List;

public class ScriptManagerFragment extends BaseSwipeBackFragment {

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_manager_script;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        setToolbarTitle("脚本管理");
        EasyRecycler<ScriptItem> recyclerView = new EasyRecycler<>(view.findViewById(R.id.recycler_view));
        List<ScriptItem> items = new ArrayList<>();
        items.add(new ScriptItem("夜间模式", "javascript: var loopCount = 0;\n" +
                "setNightMode();\n" +
                "function setNightMode() {\n" +
                "    if (document.getElementById(\"browser_night_mode_style\")) {\n" +
                "        console.log(\"has inserted and return\");\n" +
                "        return\n" +
                "    }\n" +
                "    console.log(\"begin create link element\");\n" +
                "    css = document.createElement(\"link\"),\n" +
                "    console.log(\"end create link element\");\n" +
                "    css.id = \"browser_night_mode_style\",\n" +
                "    css.rel = \"stylesheet\",\n" +
                "    css.href = 'data:text/css,html,body,table,tr,td,th,tbody,form,article,dt,ul,ol,li,dl,dd,section,footer,nav,strong,aside,header,label,address,bdo,big,blockquote,caption,center,cite,dialog,dir,fieldset,figcaption,figure,main,pre,small,h1,h2,h3,h4,h5,h6{background:#131313!important;background-image:none!important;background-color:#131313!important;color:#4E4E4E!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}div{background-color:transparent!important;color:#4E4E4E!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}div[class=\"game-icon-layer\"],div[id=\"slides\"],div[class=\"icon\"]{background:none!import2a1f12ant}p{color:#4E4E4E!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}html,body{scrollbar-base-color:#46567b!important;scrollbar-face-color:#56688f!important;scrollbar-shadow-color:#222!important;scrollbar-highlight-color:#56688f!important;scrollbar-dlight-color:#2e3952!important;scrollbar-darkshadow-color:#222!important;scrollbar-track-color:#46567b!important;scrollbar-arrow-color:#000!important;scrollbar-3dlight-color:#7a7967!important}input,select,button,textarea{box-shadow:0 0 0!important;color:#4E4E4E!important;background-color:#131313!important;border-color:#212A32!important;opacity:.5}span,em{background-color:transparent!important;color:#4E4E4E!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}input:focus,select:focus,option:focus, button:focus,textarea:focus{background-color:#131313!important;color:#59758A!important;border-color:#1A3973!important;outline:2px solid #1A3973!important}input[type=text],input[type=password]{background-image:none!important}input[type=submit],button{border:1px solid #212A32!important}img[src],input[type=image],input[type=checkbox],input[type=file]{opacity:.5}a,a *{background-color:transparent!important;color:#3B4E66!important;text-decoration:none!important;border-color:#212A32!important;text-shadow:0 0 0!important}a:visited,a:visited *{color:#0F2B47!important}a:active{color:none!important;border-color:none!important}a img{background:none!important}button.suggest-item-title{background-color:#131313!important;color:#59758a!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}div:empty,div[id=\"x-video-button\"],div[class=\"x-advert\"],div[class=\"player_controls svp_ctrl\"]{background-color:transparent!important}span,em{background-color:transparent!important;color:#59758a!important;border-color:#212A32!important;box-shadow:0 0 0!important;text-shadow:0 0 0!important}html input[type=image]:hover{opacity:1}div[class=\"img-view\"],ul[id=\"imgview\"],a[class^=\"prev\"],a[class^=\"next\"]a[class^=\"topic_img\"],a[class^=\"arrow\"],a:active[class^=\"arrow\"],a:visited[class^=\"arrow\"],img[src^=\"data\"],img[loaded=\"1\"]{background:none!important}a[class^=\"arrow\"]{height:0}.anythingSlider .arrow{background:none!important}#toolbarBox,#move_tip{background:none!important}#logolink,#mask{background-color:#131313!important;border-bottom:none!important}div::after{background-color:transparent!important}*:before,*:after{background-color:transparent!important;border-color:#212A32!important;color:#59758a!important}input::-webkit-input-placeholder{color:#59758a!important}div[class=\"x-prompt\"],div[class=\"x-dashboard\"]{background:none!important}div[class=\"x-progress-play-mini\"]{background:#eb3c10!important}div[class=\"suggest-box\"]{background:#000!important}div[class=\"x-console\"],div[class=\"x-progress\"],div[class=\"x-progress-seek\"]{background:none!important}div[class=\"x-progress-track\"]{background-color:#555555!important}div[class=\"x-progress-load\"]{background-color:#909090!important}div[class=\"x-progress-play\"],div[class=\"x-seek-handle\"]{background-color:#eb3c10!important}div[class=\"chain-con te\"],div[class=\"chain-arrow\"],div[class=\"toolbar\"],div[class=\"toolbar-con\"],div[id=\"index-box\"],div[class=\"suggest-div\"],div[class=\"suggest-box\"],div[class=\"nova-suggest\"],div[class=\"suggest-container\"],div.suggest-container.suggest-history,div[class=\"s-sugs\"],div[class=\"gstl_0 sbdd_a\"],div[class=\"se-inner\"],div[id=\"blabla-pro\"],div[id=\"fixedTitle\"],div[class=\"searchboxtop\"],div[select=\"domain\"],div[class=\"dialog\"],div[id=\"doc-link-box\"],div[id=\"searchInputBoxHistory\"],div[class=\"nearby-geolocate\"],div[class=\"popImgInr\"],div[class=\"sebox\"],div[class=\"suggest-pop\"],div[class=\"dbtg\"],div[class=\"nav-home ng-scope fixed-top\"],div[class=\"ui-suggestion-content\"],div[class=\"sw-cat\"],div[class=\"bxzbb se-sug J_SeIpt_Sug\"],input[id=\"J_searchtext\"],textarea.se-input,button[id=\"se-bn\"],button[id=\"index-bn\"],a.btn,div[class=\"weather-panel-in\"],div[class=\"schWrap fat\"],div[class=\"headerNav clearfix\"],div[class=\"foot_comment\"],s[class=\"weather-blank\"],select.weather-panel-province,select.weather-panel-city,select.weather-panel-town,div[node-type=\"box\"],div[class=\"lymb-thumb\"],a[class=\"signup_a\"],div[node-type=\"tipBox\"],div[class=\"common_search_sug\"],div[id=\"mHeader\"],div[class=\"fastli\"],div[class=\"search-box\"],div[class=\"dk_bar_sy1\"],span[id=\"wy\"],div[class=\"ui-suggestion\"],div[class=\"hot-sug\"],div[class=\"ui-suggestion-result no-result\"],div[class=\"ui-suggestion-clear\"],.pic-list li p,a.h-tab,.pic-list-n li p,div[class=\"wszh\"],.wszh span:first-child,div[class=\"ad_list\"],div[class=\"ui-suggestion-button\"],selection.tips-bar,a.sort-new,div[class=\"shareTip active\"],a#_allcomlist2,div[class=\"weather-panel-area-wrap\"],div[class=\"page transition center\"],option,#nav-view .rec li.add span:last-child,div[class=\"g-navbar ng-scope ng-isolate-scope\"],div[class=\"common_search shadow\"],div[class=\"ui-suggestion-quickdel\"],div[class=\"input-text search-area\"],div[class=\"-col-auto\"],div[class=\"locbar row -bg-light -ft-tertiary\"],div[class=\"log\"],div[class=\"head_channels\"],.channels ul li,nav[class=\"nav-mod\"],h3.weather-panel-tit,span.gbgs4,.ml-map,table.suggestions,.ml-settings-top,div[jsaction=\"settings.drag;pointerleave:settings.drag;pointerup:settings.drag;pointermove:settings.drag;pointerdown:settings.drag;touchstart:settings.drag;touchleave:settings.drag;touchmove:settings.drag;touchend:settings.drag\"],div.title,div.livetit,footer#ft,button#neighbor_getpos.pio-btn,div[id=\"cardsmanger\"],em.title_news,td.gssb_e,nav,h3,div[class=\"summary2\"],div.info,div.g-header-input-container,form.g-header-v1 .g-header-search-form,ul[class=\"cate\"],div[id=\"bd\"],div[id=\"doc\"],div.titlebar,div.from,div.input-container,input#searchInputBox.q,div.rt-startend-container,input#lineStartInputBox.rt-text,input#lineEndInputBox.rt-text,div.chart-hd,div.chart-nav2,div.nav-quirk,div.nav-main,div[class=\"mod-caizhong clearfix\"],div#hd.zst-top,div.table-more,div.input-container,input#q.q,a.down,p.tit,h2#navtit,div.head,ul.pick-betting,div.pick-c,div.pop-box,div.dg-foot,div.btm-bar,.errType textarea,div.gotop,div[class$=\"dsk\"],div.p_tabnav_nav,div.p_tabnav,div.bst_wrap,p.footer_c,div.footer,.pick-b,.index-widget-searchbox .search-area .se-input-poi,button#se-btn.btn.-brand,div.input-wrapper,input[class=\"search-input top-search-bar\"],app-card{background:#131313!important}div.ml-did-you-mean-query-correction-container{background-color:rgba(163,157,157,1)!important}textarea#q.g-header-q{border:#59758a!important}.card-wrap app-card{border-bottom:#131313!important}.-bg-normal,.g-header-v1 .g-header-search-button,header,div#tsfi.msfi,div#gbr,button.g-header-search-button,.sumext-wenda .fold-btn,.search-mod .search-btn,ui.chart-tag,ol.gbtc{background-color:#131313!important}section[class=\"switch-page-main\"],div[class=\"container shelf\"],header[class=\"hd switch-page-tab\"],div[class=\"page center current\"],div[class=\"page transition right100\"],div[class=\"container nsh\"],.pic-list li,.pick-red li span,.pick-blue li span,h1.title,ul.item,ul.pic-list,div[class=\"page-content rank-content\"],ol.rank-list,div[class=\"page-content cate-content\"],ul[class=\"classification-nav js-classify\"],.classification-nav li,.rank-list li,div[id=\"nsh-anim\"],section.anzaibody,div.info,nav.switch-page-tab-nav,li.pic,div.content,ul.item,div.container-bd,b.name,div[class=\"flyout popover_visible\"]{background-color:#131313!important}.shelf .item li,.recommended li{border-bottom:1px solid #131313!important}.tab-cont .pitch,.pick-red li .selected,.pick-red li.selected span{background-color:#E62217!important}.pick-blue li .selected{background-color:#2152b7!important}td.dg-bet-btn.dg-bet-btn-active{background:#0a9400!important}.k3bet-table td.on{background:#0e4417!important}.select-boll-list li.active .poker-num{background:#0c5322!important}.ml-searchbox input,.ml-searchbox,html input[type=button]:hover,input[type=checkbox]:hover,input[type=file]:hover,input[type=radio]:hover,input[type=reset]:hover,input[type=submit]:hover{background-color:#91979B!important}b.name,b.icon{color:#59758a!important}h1#logo{background-color:transparent!important}.weather-panel-tit{border-bottom:#131313!important}select{-webkit-appearance:none!important;box-sizing:border-box!important;align-items:center!important;border:1px solid!important;border-image-source:initial!important;border-image-slice:initial!important;border-image-width:initial!important;border-image-outset:initial!important;border-image-repeat:initial!important;white-space:pre!important;-webkit-rtl-ordering:logical!important;color:#0c5322!important;background-color:#0c5322!important}div.ml-searchbox-settings-button,td#gs_tti50.gsib_a,form[id=ml-searchboxform\"],div#J_Shade{background-color:#91979B!important}span.js-nodetail.btn.read-btn{background-color:#40c802!important}.pic_slider div img{opacity:.0!important}';\n" +
                "    var oHeads = document.getElementsByTagName(\"head\");\n" +
                "    var oBodys = document.getElementsByTagName(\"body\");\n" +
                "    loopCount++;\n" +
                "    if ((oHeads != null && oHeads.length > 0) || (oBodys != null && oBodys.length > 0)) {\n" +
                "        document.getElementsByTagName(\"head\").length != 0 ? document.getElementsByTagName(\"head\")[0].appendChild(css) : document.getElementsByTagName(\"body\")[0].appendChild(css)\n" +
                "    } else {\n" +
                "        if (loopCount < 6) {\n" +
                "            setTimeout(\"setNightMode()\", 100 * (loopCount - 1))\n" +
                "        }\n" +
                "    }\n" +
                "};"));
        items.add(new ScriptItem("日间模式", "javascript: (function() {\n" +
                "    var e = document.getElementById(\"browser_night_mode_style\");\n" +
                "    e && document.getElementsByTagName(\"head\")[0].removeChild(e);\n" +
                "    if (document.getElementById(\"day_mode_style\")) return;\n" +
                "    css = document.createElement(\"link\"),\n" +
                "    css.id = \"day_mode_style\",\n" +
                "    css.rel = \"stylesheet\",\n" +
                "    css.href = 'data:text/css,html body{background-color:#FFFFFF}',\n" +
                "    document.getElementsByTagName(\"head\").length != 0 ? document.getElementsByTagName(\"head\")[0].appendChild(css) : document.getElementsByTagName(\"body\")[0].appendChild(css)\n" +
                "})();"));
        items.add(new ScriptItem("https://neets.cc/", "{\n" +
                "      \"position\": 0,\n" +
                "      \"title\": \"Neets\",\n" +
                "      \"url\": \"https://neets.cc/\",\n" +
                "      \"icon\": \"file:///assets/neets.png\",\n" +
                "      \"type\": 0\n" +
                "    },"));
        items.add(new ScriptItem("https://www.cilimao.cc/", "{\n" +
                "      \"position\": 1,\n" +
                "      \"title\": \"磁力猫\",\n" +
                "      \"url\": \"https://www.cilimao.cc/\",\n" +
                "      \"icon\": \"file:///assets/cilimao.png\",\n" +
                "      \"type\": 0\n" +
                "    },"));
        recyclerView.setItems(items)
                .setItemRes(R.layout.item_script)
                .onBindViewHolder((holder, list, position, payloads) -> {
                    TextView patternText = holder.getView(R.id.text_pattern);
                    TextView scriptText = holder.getView(R.id.text_script);
                    patternText.setText(list.get(position).pattern);
                    scriptText.setText(list.get(position).script);
                    ClickHelper.with(holder.getItemView())
                            .setOnLongClickListener(new ClickHelper.OnLongClickListener() {
                                @Override
                                public boolean onLongClick(View v, float x, float y) {
                                    new AttachListDialogFragment<String>()
                                            .addItems("编辑", "预览", "删除")
                                            .setOnSelectListener((fragment, position1, text) -> {
                                                switch (position1) {
                                                    case 0:
                                                        break;
                                                    case 1:
                                                        break;
                                                    case 2:
                                                        break;
                                                }
                                                ZToast.normal(text);
                                                fragment.dismiss();
                                            })
                                            .setTouchPoint(x, y)
                                            .show(context);
                                    return true;
                                }
                            });
                })
                .build();
    }

    class ScriptItem {
        String pattern;
        String script;

        ScriptItem(String pattern, String script) {
            this.pattern = pattern;
            this.script = script;
        }
    }
}

