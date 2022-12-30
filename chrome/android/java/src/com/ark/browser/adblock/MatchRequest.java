package com.ark.browser.adblock;

public class MatchRequest {

    private int checkFreq;
    private String contentType;
    private String domain;
    private Boolean genericBlock;
    private String originDomain;
    private Long timeout;
    private String url;

    public String getUrl() {
        return this.url;
    }

    public void setUrl(String url) {
        this.url = url;
    }

    public String getDomain() {
        return this.domain;
    }

    public void setDomain(String domain) {
        this.domain = domain;
    }

    public String getContentType() {
        return this.contentType;
    }

    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    public String getOriginDomain() {
        return this.originDomain;
    }

    public void setOriginDomain(String originDomain) {
        this.originDomain = originDomain;
    }

    public Long getTimeout() {
        return this.timeout;
    }

    public void setTimeout(Long timeout) {
        this.timeout = timeout;
    }

    public int getCheckFreq() {
        return this.checkFreq;
    }

    public void setCheckFreq(int checkFreq) {
        this.checkFreq = checkFreq;
    }

    public Boolean isGenericBlock() {
        return this.genericBlock;
    }

    public void setGenericBlock(Boolean genericBlock) {
        this.genericBlock = genericBlock;
    }

    public boolean hasGenericBlock() {
        return this.genericBlock != null && this.genericBlock.booleanValue();
    }

}
